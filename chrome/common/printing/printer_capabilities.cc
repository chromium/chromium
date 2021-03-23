// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/strings/string_piece.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/printing/printing_buildflags.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"

#if defined(OS_WIN)
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/printing/ipp_l10n.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
#include "chrome/common/printing/print_media_l10n.h"
#if defined(OS_MAC)
#include "printing/printing_features.h"
#endif  // defined(OS_MAC)
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

namespace printing {

const char kPrinter[] = "printer";

namespace {

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
// Iterate on the `Papers` of a given printer `info` and set the
// `display_name` members, localizing where possible. We expect the
// backend to have populated non-empty display names already, so we
// don't touch media display names that we can't localize.
void PopulateAllPaperDisplayNames(PrinterSemanticCapsAndDefaults& info) {
  std::string default_paper_display =
      LocalizePaperDisplayName(info.default_paper.vendor_id);
  if (!default_paper_display.empty()) {
    info.default_paper.display_name = default_paper_display;
  }

  for (PrinterSemanticCapsAndDefaults::Paper& paper : info.papers) {
    std::string display = LocalizePaperDisplayName(paper.vendor_id);
    if (!display.empty()) {
      paper.display_name = display;
    }
  }
}
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PopulateAdvancedCapsLocalization(
    std::vector<AdvancedCapability>* advanced_capabilities) {
  auto& l10n_map = CapabilityLocalizationMap();
  for (AdvancedCapability& capability : *advanced_capabilities) {
    auto capability_it = l10n_map.find(capability.name);
    if (capability_it != l10n_map.end())
      capability.display_name = l10n_util::GetStringUTF8(capability_it->second);

    for (AdvancedCapabilityValue& value : capability.values) {
      auto value_it = l10n_map.find(capability.name + "/" + value.name);
      if (value_it != l10n_map.end())
        value.display_name = l10n_util::GetStringUTF8(value_it->second);
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Returns a dictionary representing printer capabilities as CDD, or an empty
// value if no capabilities are provided.
base::Value AssemblePrinterCapabilities(
    const std::string& device_name,
    const PrinterSemanticCapsAndDefaults::Papers& user_defined_papers,
    bool has_secure_protocol,
    PrinterSemanticCapsAndDefaults* caps) {
  DCHECK(!device_name.empty());
  if (!caps)
    return base::Value(base::Value::Type::DICTIONARY);

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
  bool populate_paper_display_names = true;
#if defined(OS_MAC)
  // Paper display name localization requires standardized vendor ID names
  // populated by CUPS IPP. If the CUPS IPP backend is not enabled, localization
  // will not properly occur.
  populate_paper_display_names =
      base::FeatureList::IsEnabled(features::kCupsIppPrintingBackend);
#endif
  if (populate_paper_display_names)
    PopulateAllPaperDisplayNames(*caps);
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

  caps->user_defined_papers = std::move(user_defined_papers);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!has_secure_protocol)
    caps->pin_supported = false;

  PopulateAdvancedCapsLocalization(&caps->advanced_capabilities);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return cloud_print::PrinterSemanticCapsAndDefaultsToCdd(*caps);
}

}  // namespace

#if defined(OS_WIN)
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

base::Value AssemblePrinterSettings(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    const PrinterSemanticCapsAndDefaults::Papers& user_defined_papers,
    bool has_secure_protocol,
    PrinterSemanticCapsAndDefaults* caps) {
  base::Value printer_info(base::Value::Type::DICTIONARY);
  printer_info.SetKey(kSettingDeviceName, base::Value(device_name));
  printer_info.SetKey(kSettingPrinterName,
                      base::Value(basic_info.display_name));
  printer_info.SetKey(kSettingPrinterDescription,
                      base::Value(basic_info.printer_description));

  base::Value options(base::Value::Type::DICTIONARY);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  printer_info.SetKey(
      kCUPSEnterprisePrinter,
      base::Value(base::Contains(basic_info.options, kCUPSEnterprisePrinter) &&
                  basic_info.options.at(kCUPSEnterprisePrinter) == kValueTrue));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  printer_info.SetKey(kSettingPrinterOptions, std::move(options));

  base::Value printer_info_capabilities(base::Value::Type::DICTIONARY);
  printer_info_capabilities.SetKey(kPrinter, std::move(printer_info));
  printer_info_capabilities.SetKey(
      kSettingCapabilities,
      AssemblePrinterCapabilities(device_name, user_defined_papers,
                                  has_secure_protocol, caps));
  return printer_info_capabilities;
}

base::Value GetSettingsOnBlockingTaskRunner(
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

  auto caps = base::make_optional<PrinterSemanticCapsAndDefaults>();
  if (!print_backend->GetPrinterSemanticCapsAndDefaults(device_name, &*caps)) {
    // Failed to get capabilities, but proceed to assemble the settings to
    // return what information we do have.
    LOG(WARNING) << "Failed to get capabilities for " << device_name;
    caps = base::nullopt;
  }

  return AssemblePrinterSettings(device_name, basic_info, user_defined_papers,
                                 has_secure_protocol,
                                 base::OptionalOrNullptr(caps));
}

}  // namespace printing
