// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include <deque>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
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
  base::ReplaceSubstringsAfterOffset(
      &stripped_guid, 0, "-", base::StringPiece());
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
#if defined(OS_WIN)
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
  for (; key_i < GetSwitchesCrashKeys().size(); ++key_i)
    GetSwitchesCrashKeys()[key_i].Clear();
}

void ResetCommandLineForTesting() {
  num_switches_key.Clear();
  for (auto& key : GetSwitchesCrashKeys()) {
    key.Clear();
  }
}

using PrinterInfoKey = crash_reporter::CrashKeyString<64>;
static PrinterInfoKey printer_info_keys[] = {
    {"prn-info-1", PrinterInfoKey::Tag::kArray},
    {"prn-info-2", PrinterInfoKey::Tag::kArray},
    {"prn-info-3", PrinterInfoKey::Tag::kArray},
    {"prn-info-4", PrinterInfoKey::Tag::kArray},
};

ScopedPrinterInfo::ScopedPrinterInfo(base::StringPiece data) {
  std::vector<base::StringPiece> info = base::SplitStringPiece(
      data, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < base::size(printer_info_keys); ++i) {
    if (i < info.size())
      printer_info_keys[i].Set(info[i]);
    else
      printer_info_keys[i].Clear();
  }
}

ScopedPrinterInfo::~ScopedPrinterInfo() {
  for (auto& crash_key : printer_info_keys) {
    crash_key.Clear();
  }
}

}  // namespace crash_keys
