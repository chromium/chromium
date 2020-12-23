// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/page_range.h"

namespace printing {

// Keys for a dictionary specifying a custom vendor capability. See
// settings/advanced_settings/advanced_settings_item.js in
// chrome/browser/resources/print_preview.
const char kOptionKey[] = "option";
const char kSelectCapKey[] = "select_cap";
const char kSelectString[] = "SELECT";
const char kTypeKey[] = "type";

// The dictionary key for the CDD item containing custom vendor capabilities.
const char kVendorCapabilityKey[] = "vendor_capability";

namespace {

void PrintersToValues(const PrinterList& printer_list,
                      base::ListValue* printers) {
  for (const PrinterBasicInfo& printer : printer_list) {
    auto printer_info = std::make_unique<base::DictionaryValue>();
    printer_info->SetString(kSettingDeviceName, printer.printer_name);

    printer_info->SetString(kSettingPrinterName, printer.display_name);
    printer_info->SetString(kSettingPrinterDescription,
                            printer.printer_description);

    auto options = std::make_unique<base::DictionaryValue>();
    for (const auto& opt_it : printer.options)
      options->SetString(opt_it.first, opt_it.second);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    printer_info->SetBoolean(
        kCUPSEnterprisePrinter,
        base::Contains(printer.options, kCUPSEnterprisePrinter) &&
            printer.options.at(kCUPSEnterprisePrinter) == kValueTrue);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    printer_info->Set(kSettingPrinterOptions, std::move(options));

    printers->Append(std::move(printer_info));

    VLOG(1) << "Found printer " << printer.display_name << " with device name "
            << printer.printer_name;
  }
}

template <typename Predicate>
base::Value GetFilteredList(const base::Value* list, Predicate pred) {
  auto out_list = list->Clone();
  out_list.EraseListValueIf(pred);
  return out_list;
}

bool ValueIsNull(const base::Value& val) {
  return val.is_none();
}

bool VendorCapabilityInvalid(const base::Value& val) {
  if (!val.is_dict())
    return true;
  const base::Value* option_type =
      val.FindKeyOfType(kTypeKey, base::Value::Type::STRING);
  if (!option_type)
    return true;
  if (option_type->GetString() != kSelectString)
    return false;
  const base::Value* select_cap =
      val.FindKeyOfType(kSelectCapKey, base::Value::Type::DICTIONARY);
  if (!select_cap)
    return true;
  const base::Value* options_list =
      select_cap->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
  if (!options_list || options_list->GetList().empty() ||
      GetFilteredList(options_list, ValueIsNull).GetList().empty()) {
    return true;
  }
  return false;
}

void SystemDialogDone(const base::Value& error) {
  // intentional no-op
}

}  // namespace

base::Value ValidateCddForPrintPreview(base::Value cdd) {
  base::Value* caps =
      cdd.FindKeyOfType(kPrinter, base::Value::Type::DICTIONARY);
  if (!caps)
    return cdd;

  base::Value out_caps(base::Value::Type::DICTIONARY);
  for (auto capability : caps->DictItems()) {
    const auto& key = capability.first;
    base::Value* value = &capability.second;

    base::Value* list_value = nullptr;
    if (value->is_dict())
      list_value = value->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
    else if (value->is_list())
      list_value = value;

    if (!list_value) {
      out_caps.SetKey(key, std::move(*value));
      continue;
    }

    bool is_vendor_capability = key == kVendorCapabilityKey;
    list_value->EraseListValueIf(is_vendor_capability ? VendorCapabilityInvalid
                                                      : ValueIsNull);
    if (list_value->GetList().empty())  // leave out empty lists.
      continue;

    if (is_vendor_capability) {
      // Need to also filter the individual capability lists.
      for (auto& vendor_option : list_value->GetList()) {
        if (*vendor_option.FindStringKey(kTypeKey) != kSelectString)
          continue;

        vendor_option.FindDictKey(kSelectCapKey)
            ->FindListKey(kOptionKey)
            ->EraseListValueIf(ValueIsNull);
      }
    }
    if (value->is_dict()) {
      base::Value option_dict(base::Value::Type::DICTIONARY);
      option_dict.SetKey(kOptionKey, std::move(*list_value));
      out_caps.SetKey(key, std::move(option_dict));
    } else {
      out_caps.SetKey(key, std::move(*list_value));
    }
  }
  cdd.SetKey(kPrinter, std::move(out_caps));
  return cdd;
}

void ConvertPrinterListForCallback(
    PrinterHandler::AddedPrintersCallback callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    const PrinterList& printer_list) {
  base::ListValue printers;
  PrintersToValues(printer_list, &printers);

  VLOG(1) << "Enumerate printers finished, found " << printers.GetSize()
          << " printers";
  if (!printers.empty())
    callback.Run(printers);
  std::move(done_callback).Run();
}

void StartLocalPrint(base::Value job_settings,
                     scoped_refptr<base::RefCountedMemory> print_data,
                     content::WebContents* preview_web_contents,
                     PrinterHandler::PrintCallback callback) {
  // Get print view manager.
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  content::WebContents* initiator =
      dialog_controller ? dialog_controller->GetInitiator(preview_web_contents)
                        : nullptr;
  PrintViewManager* print_view_manager =
      initiator ? PrintViewManager::FromWebContents(initiator) : nullptr;
  if (!print_view_manager) {
    std::move(callback).Run(base::Value("Initiator closed"));
    return;
  }

  if (job_settings.FindBoolKey(kSettingShowSystemDialog).value_or(false) ||
      job_settings.FindBoolKey(kSettingOpenPDFInPreview).value_or(false)) {
    // Run the callback early, or the modal dialogs will prevent the preview
    // from closing until they do.
    std::move(callback).Run(base::Value());
    callback = base::BindOnce(&SystemDialogDone);
  }
  print_view_manager->PrintForPrintPreview(
      std::move(job_settings), std::move(print_data),
      preview_web_contents->GetMainFrame(), std::move(callback));
}

bool ParseSettings(const base::Value& settings,
                   std::string* out_destination_id,
                   std::string* out_capabilities,
                   gfx::Size* out_page_size,
                   base::Value* out_ticket) {
  const std::string* ticket_opt = settings.FindStringKey(kSettingTicket);
  const std::string* capabilities_opt =
      settings.FindStringKey(kSettingCapabilities);
  out_page_size->SetSize(settings.FindIntKey(kSettingPageWidth).value_or(0),
                         settings.FindIntKey(kSettingPageHeight).value_or(0));
  if (!ticket_opt || !capabilities_opt || out_page_size->IsEmpty()) {
    NOTREACHED();
    return false;
  }
  base::Optional<base::Value> ticket_value =
      base::JSONReader::Read(*ticket_opt);
  if (!ticket_value)
    return false;

  *out_destination_id = *settings.FindStringKey(kSettingDeviceName);
  *out_capabilities = *capabilities_opt;
  *out_ticket = std::move(*ticket_value);
  return true;
}

}  // namespace printing
