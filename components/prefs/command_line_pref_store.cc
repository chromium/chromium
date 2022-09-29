// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/command_line_pref_store.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"

CommandLinePrefStore::CommandLinePrefStore(
    const base::CommandLine* command_line)
    : command_line_(command_line) {}

CommandLinePrefStore::~CommandLinePrefStore() {}

void CommandLinePrefStore::ApplyStringSwitches(
    const CommandLinePrefStore::SwitchToPreferenceMapEntry string_switch[],
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (command_line_->HasSwitch(string_switch[i].switch_name)) {
      SetValue(string_switch[i].preference_path,
               base::Value(command_line_->GetSwitchValueASCII(
                   string_switch[i].switch_name)),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyPathSwitches(
    const CommandLinePrefStore::SwitchToPreferenceMapEntry path_switch[],
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (command_line_->HasSwitch(path_switch[i].switch_name)) {
      SetValue(path_switch[i].preference_path,
               base::Value(
                   command_line_->GetSwitchValuePath(path_switch[i].switch_name)
                       .AsUTF8Unsafe()),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyIntegerSwitches(
    const CommandLinePrefStore::SwitchToPreferenceMapEntry integer_switch[],
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (command_line_->HasSwitch(integer_switch[i].switch_name)) {
      std::string str_value = command_line_->GetSwitchValueASCII(
          integer_switch[i].switch_name);
      int int_value = 0;
      if (!base::StringToInt(str_value, &int_value)) {
        LOG(ERROR) << "The value " << str_value << " of "
                   << integer_switch[i].switch_name
                   << " can not be converted to integer, ignoring!";
        continue;
      }
      SetValue(integer_switch[i].preference_path, base::Value(int_value),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyBooleanSwitches(
      const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
      boolean_switch_map[], size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (command_line_->HasSwitch(boolean_switch_map[i].switch_name)) {
      SetValue(boolean_switch_map[i].preference_path,
               base::Value(boolean_switch_map[i].set_value),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}
