// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "extensions/common/value_builder.h"
#include "printing/print_job_constants.h"
#include "printing/pwg_raster_settings.h"
#include "services/device/public/mojom/usb_device.mojom.h"

using extensions::DevicePermissionsManager;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ListBuilder;
using extensions::UsbDeviceManager;
using extensions::UsbPrinterManifestData;

namespace printing {

namespace {

const char kContentTypePdf[] = "application/pdf";
const char kContentTypePWGRaster[] = "image/pwg-raster";
const char kContentTypeAll[] = "*/*";

const char kInvalidDataPrintError[] = "INVALID_DATA";
const char kInvalidTicketPrintError[] = "INVALID_TICKET";

const char kProvisionalUsbLabel[] = "provisional-usb";

// Updates |job| with raster data. Returns the updated print job.
void UpdateJobFileInfo(std::unique_ptr<extensions::PrinterProviderPrintJob> job,
                       ExtensionPrinterHandler::PrintJobCallback callback,
                       base::ReadOnlySharedMemoryRegion pwg_region) {
  auto data =
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(pwg_region);
  if (data)
    job->document_bytes = data;
  std::move(callback).Run(std::move(job));
}

bool HasUsbPrinterProviderPermissions(const Extension* extension) {
  return extension->permissions_data() &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kPrinterProvider) &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kUsb);
}

std::string GenerateProvisionalUsbPrinterId(
    const Extension* extension,
    const device::mojom::UsbDeviceInfo& device) {
  return base::StringPrintf("%s:%s:%s", kProvisionalUsbLabel,
                            extension->id().c_str(), device.guid.c_str());
}

bool ParseProvisionalUsbPrinterId(const std::string& printer_id,
                                  std::string* extension_id,
                                  std::string* device_guid) {
  std::vector<std::string> components = base::SplitString(
      printer_id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (components.size() != 3)
    return false;

  if (components[0] != kProvisionalUsbLabel)
    return false;

  *extension_id = components[1];
  *device_guid = components[2];
  return true;
}

extensions::PrinterProviderAPI* GetPrinterProviderAPI(Profile* profile) {
  return extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile);
}

}  // namespace

ExtensionPrinterHandler::ExtensionPrinterHandler(Profile* profile)
    : profile_(profile) {}

ExtensionPrinterHandler::~ExtensionPrinterHandler() {
}

void ExtensionPrinterHandler::Reset() {
  // TODO(tbarzic): Keep track of pending request ids issued by |this| and
  // cancel them from here.
  pending_enumeration_count_ = 0;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionPrinterHandler::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  // Assume that there can only be one printer enumeration occuring at once.
  DCHECK_EQ(pending_enumeration_count_, 0);
  pending_enumeration_count_ = 1;
  done_callback_ = std::move(done_callback);

  bool extension_supports_usb_printers = false;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  for (const auto& extension : registry->enabled_extensions()) {
    if (UsbPrinterManifestData::Get(extension.get()) &&
        HasUsbPrinterProviderPermissions(extension.get())) {
      extension_supports_usb_printers = true;
      break;
    }
  }

  if (extension_supports_usb_printers) {
    pending_enumeration_count_++;
    UsbDeviceManager* usb_manager = UsbDeviceManager::Get(profile_);
    DCHECK(usb_manager);
    usb_manager->GetDevices(
        base::BindOnce(&ExtensionPrinterHandler::OnUsbDevicesEnumerated,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  GetPrinterProviderAPI(profile_)->DispatchGetPrintersRequested(
      base::BindRepeating(&ExtensionPrinterHandler::WrapGetPrintersCallback,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::StartGetCapability(
    const std::string& destination_id,
    GetCapabilityCallback callback) {
  GetPrinterProviderAPI(profile_)->DispatchGetCapabilityRequested(
      destination_id,
      base::BindOnce(&ExtensionPrinterHandler::WrapGetCapabilityCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::StartPrint(
    const base::string16& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  auto print_job = std::make_unique<extensions::PrinterProviderPrintJob>();
  print_job->job_title = job_title;
  std::string capabilities;
  gfx::Size page_size;
  if (!ParseSettings(settings, &print_job->printer_id, &capabilities,
                     &page_size, &print_job->ticket)) {
    std::move(callback).Run(base::Value("Invalid settings"));
    return;
  }

  cloud_devices::CloudDeviceDescription printer_description;
  printer_description.InitFromString(capabilities);

  cloud_devices::printer::ContentTypesCapability content_types;
  content_types.LoadFrom(printer_description);

  const bool use_pdf = content_types.Contains(kContentTypePdf) ||
                       content_types.Contains(kContentTypeAll);
  if (use_pdf) {
    print_job->content_type = kContentTypePdf;
    print_job->document_bytes = print_data;
    DispatchPrintJob(std::move(callback), std::move(print_job));
    return;
  }

  if (!cloud_devices::CloudDeviceDescription::IsValidTicket(
          print_job->ticket)) {
    WrapPrintCallback(std::move(callback),
                      base::Value(kInvalidTicketPrintError));
    return;
  }

  print_job->content_type = kContentTypePWGRaster;
  ConvertToPWGRaster(
      print_data, printer_description, page_size, std::move(print_job),
      base::BindOnce(&ExtensionPrinterHandler::DispatchPrintJob,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::StartGrantPrinterAccess(
    const std::string& printer_id,
    GetPrinterInfoCallback callback) {
  std::string extension_id;
  std::string device_guid;
  if (!ParseProvisionalUsbPrinterId(printer_id, &extension_id, &device_guid)) {
    std::move(callback).Run(base::DictionaryValue());
    return;
  }

  UsbDeviceManager* usb_manager = UsbDeviceManager::Get(profile_);
  DCHECK(usb_manager);

  const device::mojom::UsbDeviceInfo* device =
      usb_manager->GetDeviceInfo(device_guid);
  if (!device) {
    std::move(callback).Run(base::DictionaryValue());
    return;
  }

  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(profile_);
  permissions_manager->AllowUsbDevice(extension_id, *device);

  GetPrinterProviderAPI(profile_)->DispatchGetUsbPrinterInfoRequested(
      extension_id, *device,
      base::BindOnce(&ExtensionPrinterHandler::WrapGetPrinterInfoCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::SetPwgRasterConverterForTesting(
    std::unique_ptr<PwgRasterConverter> pwg_raster_converter) {
  pwg_raster_converter_ = std::move(pwg_raster_converter);
}

void ExtensionPrinterHandler::ConvertToPWGRaster(
    scoped_refptr<base::RefCountedMemory> data,
    const cloud_devices::CloudDeviceDescription& printer_description,
    const gfx::Size& page_size,
    std::unique_ptr<extensions::PrinterProviderPrintJob> job,
    PrintJobCallback callback) {
  if (!pwg_raster_converter_)
    pwg_raster_converter_ = PwgRasterConverter::CreateDefault();

  cloud_devices::CloudDeviceDescription ticket;
  bool ok = ticket.InitFromValue(std::move(job->ticket));
  DCHECK(ok);
  PwgRasterSettings bitmap_settings =
      PwgRasterConverter::GetBitmapSettings(printer_description, ticket);
  job->ticket = std::move(ticket).ToValue();

  pwg_raster_converter_->Start(
      data.get(),
      PwgRasterConverter::GetConversionSettings(printer_description, page_size,
                                                bitmap_settings.use_color),
      bitmap_settings,
      base::BindOnce(&UpdateJobFileInfo, std::move(job), std::move(callback)));
}

void ExtensionPrinterHandler::DispatchPrintJob(
    PrintCallback callback,
    std::unique_ptr<extensions::PrinterProviderPrintJob> print_job) {
  if (!print_job->document_bytes) {
    WrapPrintCallback(std::move(callback), base::Value(kInvalidDataPrintError));
    return;
  }

  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile_)
      ->DispatchPrintRequested(
          std::move(*print_job),
          base::BindOnce(&ExtensionPrinterHandler::WrapPrintCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::WrapGetPrintersCallback(
    AddedPrintersCallback callback,
    const base::ListValue& printers,
    bool done) {
  DCHECK_GT(pending_enumeration_count_, 0);
  if (!printers.empty())
    callback.Run(printers);

  if (done)
    pending_enumeration_count_--;
  if (pending_enumeration_count_ == 0)
    std::move(done_callback_).Run();
}

void ExtensionPrinterHandler::WrapGetCapabilityCallback(
    GetCapabilityCallback callback,
    const base::DictionaryValue& capability) {
  base::Value capabilities(base::Value::Type::DICTIONARY);
  base::Value cdd = ValidateCddForPrintPreview(capability.Clone());
  // Leave |capabilities| empty if |cdd| is empty.
  if (!cdd.DictEmpty())
    capabilities.SetKey(kSettingCapabilities, std::move(cdd));

  std::move(callback).Run(std::move(capabilities));
}

void ExtensionPrinterHandler::WrapPrintCallback(PrintCallback callback,
                                                const base::Value& status) {
  std::move(callback).Run(status);
}

void ExtensionPrinterHandler::WrapGetPrinterInfoCallback(
    GetPrinterInfoCallback callback,
    const base::DictionaryValue& printer_info) {
  std::move(callback).Run(printer_info);
}

void ExtensionPrinterHandler::OnUsbDevicesEnumerated(
    AddedPrintersCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(profile_);

  ListBuilder printer_list;

  for (const auto& extension : registry->enabled_extensions()) {
    const UsbPrinterManifestData* manifest_data =
        UsbPrinterManifestData::Get(extension.get());
    if (!manifest_data || !HasUsbPrinterProviderPermissions(extension.get()))
      continue;

    const extensions::DevicePermissions* device_permissions =
        permissions_manager->GetForExtension(extension->id());
    for (const auto& device : devices) {
      if (manifest_data->SupportsDevice(*device)) {
        std::unique_ptr<extensions::UsbDevicePermission::CheckParam> param =
            extensions::UsbDevicePermission::CheckParam::ForUsbDevice(
                extension.get(), *device);
        if (device_permissions->FindUsbDeviceEntry(*device) ||
            extension->permissions_data()->CheckAPIPermissionWithParam(
                extensions::APIPermission::kUsbDevice, param.get())) {
          // Skip devices the extension already has permission to access.
          continue;
        }

        printer_list.Append(
            DictionaryBuilder()
                .Set("id",
                     GenerateProvisionalUsbPrinterId(extension.get(), *device))
                .Set("name",
                     DevicePermissionsManager::GetPermissionMessage(
                         device->vendor_id, device->product_id,
                         device->manufacturer_name.value_or(base::string16()),
                         device->product_name.value_or(base::string16()),
                         base::string16(), false))
                .Set("extensionId", extension->id())
                .Set("extensionName", extension->name())
                .Set("provisional", true)
                .Build());
      }
    }
  }

  DCHECK_GT(pending_enumeration_count_, 0);
  pending_enumeration_count_--;
  std::unique_ptr<base::ListValue> list = printer_list.Build();
  if (!list->empty())
    callback.Run(*list);
  if (pending_enumeration_count_ == 0)
    std::move(done_callback_).Run();
}

}  // namespace printing
