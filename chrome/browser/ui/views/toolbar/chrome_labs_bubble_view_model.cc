// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/flag_descriptions.h"

ChromeLabsBubbleViewModel::ChromeLabsBubbleViewModel() {
  SetUpLabs();
}

ChromeLabsBubbleViewModel::~ChromeLabsBubbleViewModel() = default;

const std::vector<LabInfo>& ChromeLabsBubbleViewModel::GetLabInfo() const {
  return lab_info_;
}

// TODO(elainechien): Explore better ways to allow developers to add their
// experiments.
// Experiments featured in labs must have feature entries of type FEATURE_VALUE
// (Default, Enabled, Disabled states). Experiments with multiple parameters may
// be considered in the future.
void ChromeLabsBubbleViewModel::SetUpLabs() {
  // Read Later.
  lab_info_.emplace_back(LabInfo(
      flag_descriptions::kReadLaterFlagId, base::ASCIIToUTF16("Reading List"),
      base::ASCIIToUTF16(
          "Right click on a tab or click the star to add tabs to a reading "
          "list. Access from the Bookmarks bar.")));

  // Tab Search.
  lab_info_.emplace_back(
      LabInfo(flag_descriptions::kEnableTabSearchFlagId,
              base::ASCIIToUTF16("Tab Search"),
              base::ASCIIToUTF16("Enable a popup bubble in Top Chrome UI to "
                                 "search over currently open tabs.")));
}

void ChromeLabsBubbleViewModel::SetLabInfoForTesting(
    const std::vector<LabInfo>& test_feature_info) {
  lab_info_ = test_feature_info;
}
