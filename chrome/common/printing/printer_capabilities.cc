// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printer_capabilities.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/printing/printing_buildflags.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/printing/ipp_l10n.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
#include "chrome/common/printing/print_media_l10n.h"
#if BUILDFLAG(IS_MAC)
#include "printing/printing_features.h"
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

namespace printing {

const char kPrinter[] = "printer";

namespace {

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
// Iterate on the `Papers` of a given printer `info` and set the
// `display_name` members, localizing where possible. We expect the
// backend to have populated non-empty display names already, so we
// don't touch media display names that we can't localize.
// The `Papers` will be sorted in place when this function returns.
void PopulateAndSortAllPaperDisplayNames(PrinterSemanticCapsAndDefaults& info) {
  MediaSizeInfo default_paper_display =
      LocalizePaperDisplayName(info.default_paper.vendor_id);
  if (!default_paper_display.name.empty()) {
    info.default_paper.display_name =
        base::UTF16ToUTF8(default_paper_display.name);
  }

  // Pair the paper entries with their sort info so they can be sorted.
  std::vector<PaperWithSizeInfo> size_list;
  for (PrinterSemanticCapsAndDefaults::Paper& paper : info.papers) {
    size_list.emplace_back(LocalizePaperDisplayName(paper.vendor_id),
                           std::move(paper));
  }

  // Sort and recreate the list with localizations inserted.
  SortPaperDisplayNames(size_list);
  info.papers.clear();
  for (auto& pair : size_list) {
    auto& paper = info.papers.emplace_back(std::move(pair.paper));
    if (!pair.size_info.name.empty()) {
      paper.display_name = base::UTF16ToUTF8(pair.size_info.name);
    }
  }
}
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

#if BUILDFLAG(IS_CHROMEOS)
void PopulateAdvancedCapsLocalization(
    std::vector<AdvancedCapability>* advanced_capabilities) {
  auto& l10n_map = CapabilityLocalizationMap();
  for (AdvancedCapability& capability : *advanced_capabilities) {
    auto capability_it = l10n_map.find(capability.name);
    if (capability_it != l10n_map.end())
      capability.display_name = l10n_util::GetStringUTF8(capability_it->second);

    for (AdvancedCapabilityValue& value : capability.values) {
      auto value_it =
          l10n_map.find(base::StrCat({capability.name, "/", value.name}));
      if (value_it != l10n_map.end())
        value.display_name = l10n_util::GetStringUTF8(value_it->second);
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns a dictionary representing printer capabilities as CDD, or
// a Value of type NONE if no capabilities are provided.
base::Value AssemblePrinterCapabilities(
    const std::string& device_name,
    const PrinterSemanticCapsAndDefaults::Papers& user_defined_papers,
    bool has_secure_protocol,
    PrinterSemanticCapsAndDefaults* caps) {
  DCHECK(!device_name.empty());
  if (!caps)
    return base::Value();

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
  bool populate_paper_display_names = true;
#if BUILDFLAG(IS_MAC)
  // Paper display name localization requires standardized vendor ID names
  // populated by CUPS IPP. If the CUPS IPP backend is not enabled, localization
  // will not properly occur.
  populate_paper_display_names =
      base::FeatureList::IsEnabled(features::kCupsIppPrintingBackend);
#endif
  if (populate_paper_display_names)
    PopulateAndSortAllPaperDisplayNames(*caps);
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

  caps->user_defined_papers = std::move(user_defined_papers);

#if BUILDFLAG(IS_CHROMEOS)
  if (!has_secure_protocol)
    caps->pin_supported = false;

  PopulateAdvancedCapsLocalization(&caps->advanced_capabilities);
#endif  // BUILDFLAG(IS_CHROMEOS)

  return cloud_print::PrinterSemanticCapsAndDefaultsToCdd(*caps);
}

}  // namespace

#if BUILDFLAG(IS_WIN)
std::string GetUserFriendlyName(const std::string& printer_name) {
  // `printer_name` may be a UNC path like \\printserver\printername.
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

base::Value::Dict AssemblePrinterSettings(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    const PrinterSemanticCapsAndDefaults::Papers& user_defined_papers,
    bool has_secure_protocol,
    PrinterSemanticCapsAndDefaults* caps) {
  base::Value::Dict printer_info;
  printer_info.Set(kSettingDeviceName, device_name);
  printer_info.Set(kSettingPrinterName, basic_info.display_name);
  printer_info.Set(kSettingPrinterDescription, basic_info.printer_description);

  base::Value::Dict options;

#if BUILDFLAG(IS_CHROMEOS)
  printer_info.Set(
      kCUPSEnterprisePrinter,
      base::Contains(basic_info.options, kCUPSEnterprisePrinter) &&
          basic_info.options.at(kCUPSEnterprisePrinter) == kValueTrue);
#endif  // BUILDFLAG(IS_CHROMEOS)

  printer_info.Set(kSettingPrinterOptions, std::move(options));

  base::Value::Dict printer_info_capabilities;
  printer_info_capabilities.Set(kPrinter, std::move(printer_info));
  base::Value capabilities = AssemblePrinterCapabilities(
      device_name, user_defined_papers, has_secure_protocol, caps);
  if (capabilities.is_dict()) {
    printer_info_capabilities.Set(kSettingCapabilities,
                                  std::move(capabilities));
  }
  return printer_info_capabilities;
}

base::Value::Dict GetSettingsOnBlockingTaskRunner(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    PrinterSemanticCapsAndDefaults::Papers user_defined_papers,
    bool has_secure_protocol,
    scoped_refptr<PrintBackend> print_backend) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  VLOG(1) << "Get printer capabilities start for " << device_name;
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(device_name));

  auto caps = absl::make_optional<PrinterSemanticCapsAndDefaults>();
  if (print_backend->GetPrinterSemanticCapsAndDefaults(device_name, &*caps) !=
      mojom::ResultCode::kSuccess) {
    // Failed to get capabilities, but proceed to assemble the settings to
    // return what information we do have.
    LOG(WARNING) << "Failed to get capabilities for " << device_name;
    caps = absl::nullopt;
  }

  return AssemblePrinterSettings(device_name, basic_info, user_defined_papers,
                                 has_secure_protocol,
                                 base::OptionalToPtr(caps));
}

}  // namespace printing
