// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "base/strings/utf_string_conversions.h"

ChromeLabsBubbleViewModel::ChromeLabsBubbleViewModel() {
  SetUpLabs();
}

std::vector<std::string> ChromeLabsBubbleViewModel::GetLabInfo() const {
  return lab_internal_names_;
}

// TODO(elainechien): Explore better ways to allow developers to add their
// experiments.
// Experiments featured in labs must have feature entries of type FEATURE_VALUE
// (Default, Enabled, Disabled states). Experiments with multiple parameters may
// be considered in the future.
void ChromeLabsBubbleViewModel::SetUpLabs() {
  lab_internal_names_.push_back("read-later");
}

ChromeLabsBubbleViewModel::~ChromeLabsBubbleViewModel() = default;
