// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_PREFS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_PREFS_H_

class PrefRegistrySimple;

namespace embedder_support {

class OriginTrialPrefs {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_PREFS_H_
