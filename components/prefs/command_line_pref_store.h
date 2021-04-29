// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_COMMAND_LINE_PREF_STORE_H_
#define COMPONENTS_PREFS_COMMAND_LINE_PREF_STORE_H_

#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/prefs/value_map_pref_store.h"

// Base class for a PrefStore that maps command line switches to preferences.
// The Apply...Switches() methods can be called by subclasses with their own
// maps, or delegated to other code.
class COMPONENTS_PREFS_EXPORT CommandLinePrefStore : public ValueMapPrefStore {
 public:
  struct SwitchToPreferenceMapEntry {
    const char* switch_name;
    const char* preference_path;
  };

  // |set_value| indicates what the preference should be set to if the switch
  // is present.
  struct BooleanSwitchToPreferenceMapEntry {
    const char* switch_name;
    const char* preference_path;
    bool set_value;
  };

  // Apply command-line switches to the corresponding preferences of the switch
  // map, where the value associated with the switch is a string.
  void ApplyStringSwitches(
      const SwitchToPreferenceMapEntry string_switch_map[], size_t size);

  // Apply command-line switches to the corresponding preferences of the switch
  // map, where the value associated with the switch is a path.
  void ApplyPathSwitches(const SwitchToPreferenceMapEntry path_switch_map[],
                         size_t size);

  // Apply command-line switches to the corresponding preferences of the switch
  // map, where the value associated with the switch is an integer.
  void ApplyIntegerSwitches(
      const SwitchToPreferenceMapEntry integer_switch_map[], size_t size);

  // Apply command-line switches to the corresponding preferences of the
  // boolean switch map.
  void ApplyBooleanSwitches(
      const BooleanSwitchToPreferenceMapEntry boolean_switch_map[],
      size_t size);


 protected:
  explicit CommandLinePrefStore(const base::CommandLine* command_line);
  ~CommandLinePrefStore() override;

  const base::CommandLine* command_line() { return command_line_; }

 private:
  // Weak reference.
  const base::CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(CommandLinePrefStore);
};

#endif  // COMPONENTS_PREFS_COMMAND_LINE_PREF_STORE_H_
