// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/flags_ui_pref_names.h"

namespace flags_ui {
namespace prefs {

// List of names of the enabled labs experiments.
const char kAboutFlagsEntries[] = "browser.enabled_labs_experiments";

// A dictionary of (flag, origin_list) pairs. Each origin_list is a string
// containing comma concatenated, serialized Origin objects.
// This must not be a child of kEnabledLabsExperiment
// (e.g. browser.enabled_labs_experiments.origin_lists)
// otherwise it will overwrite browser.enabled_labs_experiments values.
const char kAboutFlagsOriginLists[] =
    "browser.enabled_labs_experiments_origin_lists";

}  // namespace prefs
}  // namespace flags_ui
