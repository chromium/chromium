// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/printer_capabilities.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/printing/browser/printing_buildflags.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"

#if defined(OS_WIN)
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/feature_list.h"
#include "components/printing/browser/ipp_l10n.h"
#include "components/strings/grit/components_strings.h"
#include "printing/printing_features_chromeos.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
#include "components/printing/browser/print_media_l10n.h"
#endif

namespace printing {

const char kPrinter[] = "printer";

namespace {

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
// Iterate on the Papers of a given printer |info| and set the
// display_name members, localizing where possible. We expect the
// backend to have populated non-empty display names already, so we
// don't touch media display names that we can't localize.
void PopulateAllPaperDisplayNames(PrinterSemanticCapsAndDefaults* info) {
  std::string default_paper_display =
      LocalizePaperDisplayName(info->default_paper.vendor_id);
  if (!default_paper_display.empty()) {
    info->default_paper.display_name = default_paper_display;
  }

  for (PrinterSemanticCapsAndDefaults::Paper& paper : info->papers) {
    std::string display = LocalizePaperDisplayName(paper.vendor_id);
    if (!display.empty()) {
      paper.display_name = display;
    }
  }
}
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

#if defined(OS_CHROMEOS)
void PopulateAdvancedCapsLocalization(
    std::vector<AdvancedCapability>* advanced_capabilities) {
  auto& l10n_map = CapabilityLocalizationMap();
  for (AdvancedCapability& capability : *advanced_capabilities) {
    auto it = l10n_map.find(capability.name);
    if (it != l10n_map.end())
      capability.display_name = l10n_util::GetStringUTF8(it->second);

    for (AdvancedCapabilityValue& value : capability.values) {
      auto it = l10n_map.find(capability.name + "/" + value.name);
      if (it != l10n_map.end())
        value.display_name = l10n_util::GetStringUTF8(it->second);
    }
  }
}
#endif  // defined(OS_CHROMEOS)

// Returns a dictionary representing printer capabilities as CDD.  Returns
// an empty dictionary if a dictionary could not be generated.
base::Value GetPrinterCapabilitiesOnBlockingTaskRunner(
    const std::string& device_name,
    const PrinterSemanticCapsAndDefaults::Papers& additional_papers,
    bool has_secure_protocol,
    scoped_refptr<PrintBackend> print_backend) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!device_name.empty());
  scoped_refptr<PrintBackend> backend =
      print_backend ? print_backend
                    : printing::PrintBackend::CreateInstance(nullptr);

  VLOG(1) << "Get printer capabilities start for " << device_name;
  crash_keys::ScopedPrinterInfo crash_key(
      backend->GetPrinterDriverInfo(device_name));

  PrinterSemanticCapsAndDefaults info;
  if (!backend->GetPrinterSemanticCapsAndDefaults(device_name, &info)) {
    LOG(WARNING) << "Failed to get capabilities for " << device_name;
    return base::Value(base::Value::Type::DICTIONARY);
  }

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
  PopulateAllPaperDisplayNames(&info);
#endif
  info.papers.insert(info.papers.end(), additional_papers.begin(),
                     additional_papers.end());
#if defined(OS_CHROMEOS)
  if (!has_secure_protocol)
    info.pin_supported = false;

  if (base::FeatureList::IsEnabled(printing::kAdvancedPpdAttributes))
    PopulateAdvancedCapsLocalization(&info.advanced_capabilities);
#endif  // defined(OS_CHROMEOS)

  return cloud_print::PrinterSemanticCapsAndDefaultsToCdd(info);
}

}  // namespace

#if defined(OS_WIN)
std::string GetUserFriendlyName(const std::string& printer_name) {
  // |printer_name| may be a UNC path like \\printserver\printername.
  if (!base::StartsWith(printer_name, "\\\\",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return printer_name;
  }

  // If it is a UNC path, split the "printserver\printername" portion and
  // generate a friendly name, like Windows does.
  std::string printer_name_trimmed = printer_name.substr(2);
  std::vector<std::string> tokens = base::SplitString(
      printer_name_trimmed, "\\", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 2 || tokens[0].empty() || tokens[1].empty())
    return printer_name;
  return l10n_util::GetStringFUTF8(
      IDS_PRINT_PREVIEW_FRIENDLY_WIN_NETWORK_PRINTER_NAME,
      base::UTF8ToUTF16(tokens[1]), base::UTF8ToUTF16(tokens[0]));
}
#endif

base::Value GetSettingsOnBlockingTaskRunner(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    const PrinterSemanticCapsAndDefaults::Papers& additional_papers,
    bool has_secure_protocol,
    scoped_refptr<PrintBackend> print_backend) {
  SCOPED_UMA_HISTOGRAM_TIMER("Printing.PrinterCapabilities");
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::Value printer_info(base::Value::Type::DICTIONARY);
  printer_info.SetKey(kSettingDeviceName, base::Value(device_name));
  printer_info.SetKey(kSettingPrinterName,
                      base::Value(basic_info.display_name));
  printer_info.SetKey(kSettingPrinterDescription,
                      base::Value(basic_info.printer_description));
  printer_info.SetKey(
      kCUPSEnterprisePrinter,
      base::Value(base::Contains(basic_info.options, kCUPSEnterprisePrinter) &&
                  basic_info.options.at(kCUPSEnterprisePrinter) == kValueTrue));

  base::Value printer_info_capabilities(base::Value::Type::DICTIONARY);
  printer_info_capabilities.SetKey(kPrinter, std::move(printer_info));
  printer_info_capabilities.SetKey(
      kSettingCapabilities,
      GetPrinterCapabilitiesOnBlockingTaskRunner(
          device_name, additional_papers, has_secure_protocol, print_backend));
  return printer_info_capabilities;
}

}  // namespace printing
