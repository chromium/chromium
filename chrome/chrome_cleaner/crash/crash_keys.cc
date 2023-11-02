// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/crash/crash_keys.h"

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"

namespace chrome_cleaner {

namespace {

base::Lock& GetCrashKeyLock() {
  static base::Lock* crash_key_lock = new base::Lock();
  return *crash_key_lock;
}

crashpad::SimpleStringDictionary* GetCrashKeys() {
  // TODO(crbug.com/870715): Use the new crash key API from
  // https://cs.chromium.org/chromium/src/components/crash/core/common/crash_key.h
  static crashpad::SimpleStringDictionary* crash_keys =
      new crashpad::SimpleStringDictionary();
  return crash_keys;
}

void SetCrashKeyInDictionary(crashpad::SimpleStringDictionary* crash_keys,
                             base::StringPiece key,
                             base::StringPiece value) {
  if (crash_keys->GetValueForKey(key))
    LOG(WARNING) << "Crash key \"" << key << "\" being overwritten";
  else
    DCHECK_LT(crash_keys->GetCount(), crash_keys->num_entries);
  crash_keys->SetKeyValue(key, value);
}

}  // namespace

void SetCrashKey(base::StringPiece key, base::StringPiece value) {
  base::AutoLock lock(GetCrashKeyLock());
  SetCrashKeyInDictionary(GetCrashKeys(), key, value);
}

void SetCrashKeysFromCommandLine() {
  // Based on Chrome's crash_keys::SetSwitchesFromCommandLine.
  static constexpr size_t kMaxArgs = 16;
  static constexpr char kKeyFormat[] = "CommandLineArg-%02" PRIuS;
  static constexpr char kSizeKey[] = "CommandLineSize";

  base::AutoLock lock(GetCrashKeyLock());
  crashpad::SimpleStringDictionary* crash_keys = GetCrashKeys();

  // Make sure this was only called once.
  DCHECK(!crash_keys->GetValueForKey(kSizeKey));

  const base::CommandLine::StringVector& argv =
      base::CommandLine::ForCurrentProcess()->argv();

  // Record the true number of arguments in case there are too many to store.
  SetCrashKeyInDictionary(crash_keys, kSizeKey,
                          base::NumberToString(argv.size()));

  // Go through the argv, including the exec path in argv[0]. Stop if there are
  // too many arguments to hold in crash keys.
  for (size_t key_i = 0; key_i < argv.size() && key_i < kMaxArgs; ++key_i) {
    SetCrashKeyInDictionary(crash_keys, base::StringPrintf(kKeyFormat, key_i),
                            base::WideToUTF8(argv[key_i]));
  }
}

void UseCrashKeysToAnnotate(crashpad::CrashpadInfo* crashpad_info) {
  crashpad_info->set_simple_annotations(GetCrashKeys());
}

}  // namespace chrome_cleaner
