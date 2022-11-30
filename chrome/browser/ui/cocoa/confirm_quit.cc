// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/confirm_quit.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace confirm_quit {

void RecordHistogram(ConfirmQuitMetric sample) {
  UMA_HISTOGRAM_ENUMERATION("OSX.ConfirmToQuit", sample, kSampleCount);
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kConfirmToQuitEnabled, true);
}

}  // namespace confirm_quit
