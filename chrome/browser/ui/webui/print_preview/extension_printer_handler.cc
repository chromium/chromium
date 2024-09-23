// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
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
#include "printing/print_job_constants.h"
#include "printing/pwg_raster_settings.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "ui/gfx/geometry/size.h"

using extensions::DevicePermissionsManager;
using extensions::Extension;
using extensions::ExtensionRegistry;
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
             extensions::mojom::APIPermissionID::kPrinterProvider) &&
         extension->permissions_data()->HasAPIPermission(
             extensions::mojom::APIPermissionID::kUsb);
}

std::string GenerateProvisionalUsbPrinterId(
    const Extension* extension,
    const device::mojom::UsbDeviceInfo& device) {
  return base::StringPrintf("%s:%s:%s", kProvisionalUsbLabel,
                            extension->id().c_str(), device.guid.c_str());
}

struct ProvisionalUsbPrinter {
  std::string extension_id;
  std::string device_guid;
};

std::optional<ProvisionalUsbPrinter> ParseProvisionalUsbPrinterId(
    const std::string& printer_id) {
  std::vector<std::string> components = base::SplitString(
      printer_id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != 3 || components[0] != kProvisionalUsbLabel)
    return std::nullopt;
  return ProvisionalUsbPrinter{.extension_id = std::move(components[1]),
                               .device_guid = std::move(components[2])};
}

extensions::PrinterProviderAPI* GetPrinterProviderAPI(Profile* profile) {
  return extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile);
}

struct ExtensionPrinterSettings {
  ExtensionPrinterSettings() = default;
  ExtensionPrinterSettings(ExtensionPrinterSettings&&) noexcept = default;
  ExtensionPrinterSettings& operator=(ExtensionPrinterSettings&&) noexcept =
      default;
  ~ExtensionPrinterSettings() = default;

  std::string destination_id;
  std::string capabilities;
  gfx::Size page_size;
  base::Value::Dict ticket;
};

// Parses print job `settings` for an extension printer and returns the parsed
// output. Note that `settings` is created by the Print Preview TS code, so if
// this function triggers a crash, that means the TS code and the C++ code are
// out of sync.
ExtensionPrinterSettings ParseExtensionPrinterSettings(
    const base::Value::Dict& settings) {
  ExtensionPrinterSettings parsed_settings;
  parsed_settings.destination_id = *settings.FindString(kSettingDeviceName);
  parsed_settings.capabilities = *settings.FindString(kSettingCapabilities);
  parsed_settings.page_size.SetSize(
      settings.FindInt(kSettingPageWidth).value_or(0),
      settings.FindInt(kSettingPageHeight).value_or(0));
  CHECK(!parsed_settings.page_size.IsEmpty());
  parsed_settings.ticket =
      *base::JSONReader::ReadDict(*settings.FindString(kSettingTicket));
  return parsed_settings;
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
  PRINTER_LOG(EVENT) << "ExtensionPrinterHandler::StartGetPrinters() called";

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
    PRINTER_LOG(EVENT) << "ExtensionPrinterHandler::StartGetPrinters() - "
                       << "usb printers detected";
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
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  ExtensionPrinterSettings parsed_settings =
      ParseExtensionPrinterSettings(settings);

  auto print_job = std::make_unique<extensions::PrinterProviderPrintJob>();
  print_job->printer_id = std::move(parsed_settings.destination_id);
  print_job->job_title = job_title;
  print_job->ticket = std::move(parsed_settings.ticket);

  cloud_devices::CloudDeviceDescription printer_description;
  printer_description.InitFromString(parsed_settings.capabilities);

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

  cloud_devices::CloudDeviceDescription ticket;
  if (!ticket.InitFromValue(print_job->ticket.Clone())) {
    WrapPrintCallback(std::move(callback),
                      base::Value(kInvalidTicketPrintError));
    return;
  }

  print_job->content_type = kContentTypePWGRaster;
  ConvertToPWGRaster(
      print_data, printer_description, ticket, parsed_settings.page_size,
      std::move(print_job),
      base::BindOnce(&ExtensionPrinterHandler::DispatchPrintJob,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::StartGrantPrinterAccess(
    const std::string& printer_id,
    GetPrinterInfoCallback callback) {
  std::optional<ProvisionalUsbPrinter> printer =
      ParseProvisionalUsbPrinterId(printer_id);
  if (!printer.has_value()) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  const device::mojom::UsbDeviceInfo* device =
      UsbDeviceManager::Get(profile_)->GetDeviceInfo(
          printer.value().device_guid);
  if (!device) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(profile_);
  permissions_manager->AllowUsbDevice(printer.value().extension_id, *device);

  GetPrinterProviderAPI(profile_)->DispatchGetUsbPrinterInfoRequested(
      printer.value().extension_id, *device,
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
    const cloud_devices::CloudDeviceDescription& print_ticket,
    const gfx::Size& page_size,
    std::unique_ptr<extensions::PrinterProviderPrintJob> job,
    PrintJobCallback callback) {
  if (!pwg_raster_converter_)
    pwg_raster_converter_ = PwgRasterConverter::CreateDefault();

  PwgRasterSettings bitmap_settings =
      PwgRasterConverter::GetBitmapSettings(printer_description, print_ticket);

  std::optional<bool> use_skia;
  const PrefService* prefs = profile_->GetPrefs();
  if (prefs && prefs->IsManagedPreference(prefs::kPdfUseSkiaRendererEnabled)) {
    use_skia = prefs->GetBoolean(prefs::kPdfUseSkiaRendererEnabled);
  }

  pwg_raster_converter_->Start(
      use_skia, data.get(),
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
    base::Value::List printers,
    bool done) {
  DCHECK_GT(pending_enumeration_count_, 0);
  PRINTER_LOG(EVENT) << "ExtensionPrinterHandler::WrapGetPrintersCallback(): "
                     << "printers.size()=" << printers.size()
                     << " done=" << done;
  if (!printers.empty())
    callback.Run(std::move(printers));

  if (done)
    pending_enumeration_count_--;
  if (pending_enumeration_count_ == 0)
    std::move(done_callback_).Run();
}

void ExtensionPrinterHandler::WrapGetCapabilityCallback(
    GetCapabilityCallback callback,
    base::Value::Dict capability) {
  base::Value::Dict capabilities;
  base::Value::Dict cdd = ValidateCddForPrintPreview(std::move(capability));
  // Leave |capabilities| empty if |cdd| is empty.
  if (!cdd.empty()) {
    capabilities.Set(kSettingCapabilities,
                     UpdateCddWithDpiIfMissing(std::move(cdd)));
  }

  std::move(callback).Run(std::move(capabilities));
}

void ExtensionPrinterHandler::WrapPrintCallback(PrintCallback callback,
                                                const base::Value& status) {
  std::move(callback).Run(status);
}

void ExtensionPrinterHandler::WrapGetPrinterInfoCallback(
    GetPrinterInfoCallback callback,
    base::Value::Dict printer_info) {
  std::move(callback).Run(std::move(printer_info));
}

void ExtensionPrinterHandler::OnUsbDevicesEnumerated(
    AddedPrintersCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  PRINTER_LOG(EVENT) << "ExtensionPrinterHandler::OnUsbDevicesEnumerated() "
                     << " called";
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(profile_);

  base::Value::List printer_list;

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
                extensions::mojom::APIPermissionID::kUsbDevice, param.get())) {
          // Skip devices the extension already has permission to access.
          continue;
        }

        printer_list.Append(
            base::Value::Dict()
                .Set("id",
                     GenerateProvisionalUsbPrinterId(extension.get(), *device))
                .Set("name",
                     DevicePermissionsManager::GetPermissionMessage(
                         device->vendor_id, device->product_id,
                         device->manufacturer_name.value_or(std::u16string()),
                         device->product_name.value_or(std::u16string()),
                         std::u16string(), false))
                .Set("extensionId", extension->id())
                .Set("extensionName", extension->name())
                .Set("provisional", true));
      }
    }
  }

  DCHECK_GT(pending_enumeration_count_, 0);
  pending_enumeration_count_--;
  base::Value::List list = std::move(printer_list);
  if (!list.empty())
    callback.Run(std::move(list));
  if (pending_enumeration_count_ == 0)
    std::move(done_callback_).Run();
}

}  // namespace printing
