// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_LOCAL_STATE_HELPER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_LOCAL_STATE_HELPER_H_

#include "base/time/time.h"

class PrefService;

namespace segmentation_platform {

// A helper class for keeping track of pref service entries from browser local
// state.
class LocalStateHelper {
 public:
  static LocalStateHelper& GetInstance();

  // Initializes the PrefService that is used by this object.
  // Needs to be called before calling all the get and set
  // methods.
  virtual void Initialize(PrefService* local_state) = 0;

  // Sets and gets time in local state for the given pref name.
  virtual void SetPrefTime(const char* pref_name, base::Time time) = 0;
  virtual base::Time GetPrefTime(const char* pref_name) const = 0;

  virtual ~LocalStateHelper() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_LOCAL_STATE_HELPER_H_