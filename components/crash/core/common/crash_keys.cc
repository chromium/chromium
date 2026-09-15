// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include <algorithm>
#include <array>
#include <deque>
#include <string_view>
#include <vector>

#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_key.h"

namespace crash_keys {

namespace {

#if !BUILDFLAG(USE_CRASHPAD_ANNOTATION)
// When using Crashpad, the crash reporting client ID is the responsibility of
// Crashpad. It is not set directly by Chrome. When using Breakpad instead of
// Crashpad, the crash reporting client ID is the same as the metrics client ID.
const char kMetricsClientId[] = "guid";

crash_reporter::CrashKeyString<40> client_id_key(kMetricsClientId);
#endif

}  // namespace

void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid) {
#if !BUILDFLAG(USE_CRASHPAD_ANNOTATION)
  std::string stripped_guid(metrics_client_guid);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  base::ReplaceSubstringsAfterOffset(&stripped_guid, 0, "-",
                                     std::string_view());
  if (stripped_guid.empty())
    return;

  client_id_key.Set(stripped_guid);
#endif
}

void ClearMetricsClientId() {
  // Breakpad cannot be enabled or disabled without an application restart, and
  // it needs to use the metrics client ID as its stable crash client ID, so
  // leave its client ID intact even when metrics reporting is disabled while
  // the application is running.
}

using SwitchesCrashKeys = std::deque<crash_reporter::CrashKeyString<64>>;
SwitchesCrashKeys& GetSwitchesCrashKeys() {
  static base::NoDestructor<SwitchesCrashKeys> switches_keys;
  return *switches_keys;
}

static crash_reporter::CrashKeyString<4> num_switches_key("num-switches");

void SetSwitchesFromCommandLine(const base::CommandLine& command_line,
                                SwitchFilterFunction skip_filter) {
  const base::CommandLine::StringVector& argv = command_line.argv();

  // Set the number of switches in case of uninteresting switches in
  // command_line.
  num_switches_key.Set(base::NumberToString(argv.size() - 1));

  size_t key_i = 0;

  // Go through the argv, skipping the exec path. Stop if there are too many
  // switches to hold in crash keys.
  for (size_t i = 1; i < argv.size(); ++i) {
#if BUILDFLAG(IS_WIN)
    std::string switch_str = base::WideToUTF8(argv[i]);
#else
    std::string switch_str = argv[i];
#endif

    // Skip uninteresting switches.
    if (skip_filter && (*skip_filter)(switch_str))
      continue;

    if (key_i >= GetSwitchesCrashKeys().size()) {
      static base::NoDestructor<std::deque<std::string>> crash_keys_names;
      crash_keys_names->emplace_back(
          base::StringPrintf("switch-%" PRIuS, key_i + 1));
      GetSwitchesCrashKeys().emplace_back(crash_keys_names->back().c_str());
    }
    GetSwitchesCrashKeys()[key_i++].Set(switch_str);
  }

  // Clear any remaining switches.
  for (; key_i < GetSwitchesCrashKeys().size(); ++key_i) {
    GetSwitchesCrashKeys()[key_i].Clear();
  }
}

CrashKeyWithName::CrashKeyWithName(std::string name)
    : name_(std::move(name)), crash_key_(name_.c_str()) {}

// --enable-features and --disable-features often contain a long list not
// fitting into 64 bytes, hiding important information when analysing crashes.
// Therefore they are separated out in a list of CrashKeys, one for each
// enabled or disabled feature.
// They are also excluded from the default "switches".
namespace {

using FeaturesCrashKeys = std::deque<CrashKeyWithName>;

FeaturesCrashKeys& GetEnabledFeaturesCrashKeys() {
  static base::NoDestructor<FeaturesCrashKeys> enabled_features_keys;
  return *enabled_features_keys;
}

FeaturesCrashKeys& GetDisabledFeaturesCrashKeys() {
  static base::NoDestructor<FeaturesCrashKeys> disabled_features_keys;
  return *disabled_features_keys;
}

void SplitAndPopulateFeatureCrashKeys(
    FeaturesCrashKeys& crash_keys,
    std::string_view comma_separated_feature_list,
    std::string_view crash_key_name_prefix) {
  // Crash keys are indestructible so we cannot simply empty the deque.
  // Instead we must keep the previous crash keys alive and clear their values.
  for (CrashKeyWithName& crash_key : crash_keys) {
    crash_key.Clear();
  }

  std::vector<std::string_view> features =
      base::SplitStringPiece(comma_separated_feature_list, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (size_t i = 0; i < features.size(); ++i) {
    if (crash_keys.size() <= i) {
      crash_keys.emplace_back(base::StrCat(
          {crash_key_name_prefix, "-", base::NumberToString(i + 1)}));
    }
    crash_keys[i].Set(features[i]);
  }
}

}  // namespace

void SetFeaturesFromCommandLine(const base::CommandLine& command_line) {
  SplitAndPopulateFeatureCrashKeys(
      GetEnabledFeaturesCrashKeys(),
      command_line.GetSwitchValueASCII(switches::kEnableFeatures),
      "commandline-enabled-feature");

  SplitAndPopulateFeatureCrashKeys(
      GetDisabledFeaturesCrashKeys(),
      command_line.GetSwitchValueASCII(switches::kDisableFeatures),
      "commandline-disabled-feature");
}

bool IsDefaultBoringSwitch(const std::string& flag) {
  static const auto kIgnoreSwitches = std::to_array<std::string_view>({
      switches::kEnableFeatures,
      switches::kDisableFeatures,
      // Specified as raw strings to avoid a dependency on
      // //components/webui/flags.
      "flag-switches-begin",
      "flag-switches-end",
  });

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE)) {
    return false;
  }
  size_t end = flag.find('=');
  std::string_view switch_name = std::string_view(flag).substr(
      2, end == std::string::npos ? std::string::npos : end - 2);
  return std::ranges::contains(kIgnoreSwitches, switch_name);
}

void ResetCommandLineForTesting() {
  num_switches_key.Clear();
  for (auto& key : GetSwitchesCrashKeys()) {
    key.Clear();
  }
  for (auto& key : GetEnabledFeaturesCrashKeys()) {
    key.Clear();
  }
  for (auto& key : GetDisabledFeaturesCrashKeys()) {
    key.Clear();
  }
}

using PrinterInfoKey = crash_reporter::CrashKeyString<64>;
static std::array<PrinterInfoKey, 4> printer_info_keys = {{
    {"prn-info-1", PrinterInfoKey::Tag::kArray},
    {"prn-info-2", PrinterInfoKey::Tag::kArray},
    {"prn-info-3", PrinterInfoKey::Tag::kArray},
    {"prn-info-4", PrinterInfoKey::Tag::kArray},
}};

ScopedPrinterInfo::ScopedPrinterInfo(const std::string& printer_name,
                                     std::vector<std::string> data) {
  CHECK_LE(data.size(), std::size(printer_info_keys));
  for (size_t i = 0; i < std::size(printer_info_keys); ++i) {
    if (i < data.size()) {
      printer_info_keys[i].Set(data[i]);
    } else {
      printer_info_keys[i].Clear();
    }
  }
  if (data.empty()) {
    // No keys were provided.  Just store the printer_name.
    printer_info_keys[0].Set(printer_name);
  }
}

ScopedPrinterInfo::~ScopedPrinterInfo() {
  for (auto& crash_key : printer_info_keys) {
    crash_key.Clear();
  }
}

}  // namespace crash_keys
