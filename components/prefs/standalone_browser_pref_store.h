// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_STANDALONE_BROWSER_PREF_STORE_H_
#define COMPONENTS_PREFS_STANDALONE_BROWSER_PREF_STORE_H_

#include "components/prefs/prefs_export.h"
#include "components/prefs/value_map_pref_store.h"

// A PrefStore implementation that holds preferences sent by another browser
// instance. For example, in ash, this prefstore holds the value of
// extension-controlled system prefs set in lacros (e.g. the screen magnifier)
// TODO(crbug.com/1218145): Implement persistence.
class COMPONENTS_PREFS_EXPORT StandaloneBrowserPrefStore
    : public ValueMapPrefStore {
 public:
  StandaloneBrowserPrefStore() = default;
  StandaloneBrowserPrefStore(const StandaloneBrowserPrefStore&) = delete;
  StandaloneBrowserPrefStore& operator=(const StandaloneBrowserPrefStore&) =
      delete;

 protected:
  ~StandaloneBrowserPrefStore() override;
};

#endif  // COMPONENTS_PREFS_STANDALONE_BROWSER_PREF_STORE_H_
