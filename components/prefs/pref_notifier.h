// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_NOTIFIER_H_
#define COMPONENTS_PREFS_PREF_NOTIFIER_H_

#include <string_view>

// Delegate interface used by PrefValueStore to notify its owner about changes
// to the preference values.
// TODO(mnissler, danno): Move this declaration to pref_value_store.h once we've
// cleaned up all public uses of this interface.
class PrefNotifier {
 public:
  virtual ~PrefNotifier() = default;

  // Sends out a change notification for the preference identified by
  // |pref_name|.
  virtual void OnPreferenceChanged(std::string_view pref_name) = 0;

  // Broadcasts the intialization completed notification.
  virtual void OnInitializationCompleted(bool succeeded) = 0;
};

#endif  // COMPONENTS_PREFS_PREF_NOTIFIER_H_
