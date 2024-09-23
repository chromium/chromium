// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
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
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

// Keys for a dictionary specifying a custom vendor capability. See
// settings/advanced_settings/advanced_settings_item.js in
// chrome/browser/resources/print_preview.
const char kOptionKey[] = "option";
const char kResetToDefaultKey[] = "reset_to_default";
const char kSelectCapKey[] = "select_cap";
const char kSelectString[] = "SELECT";
const char kTypeKey[] = "type";

// TODO(thestig): Consolidate duplicate constants.
const char kDpiCapabilityKey[] = "dpi";
const char kHorizontalDpi[] = "horizontal_dpi";
const char kVerticalDpi[] = "vertical_dpi";
const char kMediaSizeKey[] = "media_size";
const char kIsContinuousFeed[] = "is_continuous_feed";

// The dictionary key for the CDD item containing custom vendor capabilities.
const char kVendorCapabilityKey[] = "vendor_capability";

namespace {

base::Value::List PrintersToValues(const PrinterList& printer_list) {
  base::Value::List results;
  for (const PrinterBasicInfo& printer : printer_list) {
    base::Value::Dict printer_info;
    printer_info.Set(kSettingDeviceName, printer.printer_name);

    printer_info.Set(kSettingPrinterName, printer.display_name);
    printer_info.Set(kSettingPrinterDescription, printer.printer_description);

    base::Value::Dict options;
    for (const auto& opt_it : printer.options)
      options.SetByDottedPath(opt_it.first, opt_it.second);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    printer_info.Set(
        kCUPSEnterprisePrinter,
        base::Contains(printer.options, kCUPSEnterprisePrinter) &&
            printer.options.at(kCUPSEnterprisePrinter) == kValueTrue);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    printer_info.Set(kSettingPrinterOptions, std::move(options));

    results.Append(std::move(printer_info));

    VLOG(1) << "Found printer " << printer.display_name << " with device name "
            << printer.printer_name;
  }
  return results;
}

template <typename Predicate>
base::Value::List GetFilteredList(const base::Value::List& list,
                                  Predicate pred) {
  auto out_list = list.Clone();
  out_list.EraseIf(pred);
  return out_list;
}

bool ValueIsNull(const base::Value& val) {
  return val.is_none();
}

bool DpiCapabilityInvalid(const base::Value& val) {
  if (!val.is_dict())
    return true;
  const auto& dict = val.GetDict();
  std::optional<int> horizontal_dpi = dict.FindInt(kHorizontalDpi);
  if (horizontal_dpi.value_or(0) <= 0)
    return true;
  std::optional<int> vertical_dpi = dict.FindInt(kVerticalDpi);
  if (vertical_dpi.value_or(0) <= 0)
    return true;
  return false;
}

bool VendorCapabilityInvalid(const base::Value& val) {
  if (!val.is_dict())
    return true;
  const auto& dict = val.GetDict();
  const std::string* option_type = dict.FindString(kTypeKey);
  if (!option_type)
    return true;
  if (*option_type != kSelectString)
    return false;
  const base::Value::Dict* select_cap = dict.FindDict(kSelectCapKey);
  if (!select_cap)
    return true;
  const base::Value::List* options_list = select_cap->FindList(kOptionKey);
  if (!options_list || options_list->empty() ||
      GetFilteredList(*options_list, ValueIsNull).empty()) {
    return true;
  }
  return false;
}

void SystemDialogDone(const base::Value& error) {
  // intentional no-op
}

}  // namespace

base::Value::Dict ValidateCddForPrintPreview(base::Value::Dict cdd) {
  base::Value::Dict* caps = cdd.FindDict(kPrinter);
  if (!caps)
    return cdd;

  base::Value::Dict out_caps;
  for (auto capability : *caps) {
    const auto& key = capability.first;
    base::Value& value = capability.second;
    base::Value::List* list_value = nullptr;
    if (value.is_list())
      list_value = &value.GetList();
    if (value.is_dict())
      list_value = value.GetDict().FindList(kOptionKey);

    if (!list_value) {
      out_caps.Set(key, std::move(value));
      continue;
    }

    bool is_vendor_capability = key == kVendorCapabilityKey;
    bool is_dpi_capability = key == kDpiCapabilityKey;
    if (is_vendor_capability) {
      list_value->EraseIf(VendorCapabilityInvalid);
    } else if (is_dpi_capability) {
      list_value->EraseIf(DpiCapabilityInvalid);
    } else {
      list_value->EraseIf(ValueIsNull);
    }
    if (list_value->empty())  // leave out empty lists.
      continue;

    if (is_vendor_capability) {
      // Need to also filter the individual capability lists.
      for (auto& vendor_option : *list_value) {
        if (!vendor_option.is_dict())
          continue;

        auto& vendor_dict = vendor_option.GetDict();
        const std::string* type = vendor_dict.FindString(kTypeKey);
        if (!type || *type != kSelectString)
          continue;

        auto* select_cap_dict = vendor_dict.FindDict(kSelectCapKey);
        if (select_cap_dict) {
          auto* option_list = select_cap_dict->FindList(kOptionKey);
          if (option_list)
            option_list->EraseIf(ValueIsNull);
        }
      }
    }
    if (value.is_dict()) {
      base::Value::Dict option_dict;
      option_dict.Set(kOptionKey, std::move(*list_value));
      std::optional<bool> reset_to_default =
          value.GetDict().FindBool(kResetToDefaultKey);
      if (reset_to_default.has_value())
        option_dict.Set(kResetToDefaultKey, reset_to_default.value());
      out_caps.Set(key, std::move(option_dict));
    } else {
      out_caps.Set(key, std::move(*list_value));
    }
  }
  cdd.Set(kPrinter, std::move(out_caps));
  return cdd;
}

base::Value::Dict UpdateCddWithDpiIfMissing(base::Value::Dict cdd) {
  base::Value::Dict* printer = cdd.FindDict(kPrinter);
  if (!printer)
    return cdd;

  if (!printer->FindDict(kDpiCapabilityKey)) {
    base::Value::Dict default_dpi;
    default_dpi.Set(kHorizontalDpi, kDefaultPdfDpi);
    default_dpi.Set(kVerticalDpi, kDefaultPdfDpi);
    base::Value::List dpi_options;
    dpi_options.Append(std::move(default_dpi));
    base::Value::Dict dpi_capability;
    dpi_capability.Set(kOptionKey, std::move(dpi_options));
    printer->Set(kDpiCapabilityKey, std::move(dpi_capability));
  }
  return cdd;
}

const base::Value::List* GetMediaSizeOptionsFromCdd(
    const base::Value::Dict& cdd) {
  const base::Value::Dict* printer = cdd.FindDict(kPrinter);
  if (!printer) {
    return nullptr;
  }
  const base::Value::Dict* media_size = printer->FindDict(kMediaSizeKey);
  if (!media_size) {
    return nullptr;
  }
  return media_size->FindList(kOptionKey);
}

void FilterContinuousFeedMediaSizes(base::Value::Dict& cdd) {
  // OK to const_cast here since `cdd` started off non-const.
  base::Value::List* options =
      const_cast<base::Value::List*>(GetMediaSizeOptionsFromCdd(cdd));
  if (!options) {
    return;
  }

  options->EraseIf([](const base::Value& item) {
    const base::Value::Dict* item_dict = item.GetIfDict();
    if (!item_dict) {
      return false;
    }
    std::optional<bool> is_continuous = item_dict->FindBool(kIsContinuousFeed);
    return is_continuous.value_or(false);
  });
}

void ConvertPrinterListForCallback(
    PrinterHandler::AddedPrintersCallback callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    const PrinterList& printer_list) {
  base::Value::List printers = PrintersToValues(printer_list);

  VLOG(1) << "Enumerate printers finished, found " << printers.size()
          << " printers";
  if (!printers.empty())
    callback.Run(std::move(printers));
  std::move(done_callback).Run();
}

void StartLocalPrint(base::Value::Dict job_settings,
                     scoped_refptr<base::RefCountedMemory> print_data,
                     content::WebContents* preview_web_contents,
                     PrinterHandler::PrintCallback callback) {
  // Get print view manager.
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  content::WebContents* initiator =
      dialog_controller->GetInitiator(preview_web_contents);
  PrintViewManager* print_view_manager =
      initiator ? PrintViewManager::FromWebContents(initiator) : nullptr;
  if (!print_view_manager) {
    std::move(callback).Run(base::Value("Initiator closed"));
    return;
  }

  if (job_settings.FindBool(kSettingShowSystemDialog).value_or(false) ||
      job_settings.FindBool(kSettingOpenPDFInPreview).value_or(false)) {
    // Run the callback early, or the modal dialogs will prevent the preview
    // from closing until they do.
    std::move(callback).Run(base::Value());
    callback = base::BindOnce(&SystemDialogDone);
  }
  print_view_manager->PrintForPrintPreview(
      std::move(job_settings), std::move(print_data),
      preview_web_contents->GetPrimaryMainFrame(), std::move(callback));
}

}  // namespace printing
