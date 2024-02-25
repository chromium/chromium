// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/command_line_pref_store.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"

CommandLinePrefStore::CommandLinePrefStore(
    const base::CommandLine* command_line)
    : command_line_(command_line) {}

CommandLinePrefStore::~CommandLinePrefStore() = default;

void CommandLinePrefStore::ApplyStringSwitches(
    base::span<const CommandLinePrefStore::SwitchToPreferenceMapEntry>
        string_switch_map) {
  for (const auto& entry : string_switch_map) {
    if (command_line_->HasSwitch(entry.switch_name)) {
      SetValue(
          entry.preference_path,
          base::Value(command_line_->GetSwitchValueASCII(entry.switch_name)),
          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyPathSwitches(
    base::span<const CommandLinePrefStore::SwitchToPreferenceMapEntry>
        path_switch_map) {
  for (const auto& entry : path_switch_map) {
    if (command_line_->HasSwitch(entry.switch_name)) {
      SetValue(entry.preference_path,
               base::Value(command_line_->GetSwitchValuePath(entry.switch_name)
                               .AsUTF8Unsafe()),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyIntegerSwitches(
    base::span<const CommandLinePrefStore::SwitchToPreferenceMapEntry>
        integer_switch_map) {
  for (const auto& entry : integer_switch_map) {
    if (command_line_->HasSwitch(entry.switch_name)) {
      std::string str_value =
          command_line_->GetSwitchValueASCII(entry.switch_name);
      int int_value = 0;
      if (!base::StringToInt(str_value, &int_value)) {
        LOG(ERROR) << "The value " << str_value << " of " << entry.switch_name
                   << " can not be converted to integer, ignoring!";
        continue;
      }
      SetValue(entry.preference_path, base::Value(int_value),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyBooleanSwitches(
    base::span<const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry>
        boolean_switch_map) {
  for (const auto& entry : boolean_switch_map) {
    if (command_line_->HasSwitch(entry.switch_name)) {
      SetValue(entry.preference_path, base::Value(entry.set_value),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}
