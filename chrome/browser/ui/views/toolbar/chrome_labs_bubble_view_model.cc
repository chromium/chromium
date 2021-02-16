// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/flag_descriptions.h"

namespace {

base::Optional<std::vector<LabInfo>>& GetTestData() {
  static base::NoDestructor<base::Optional<std::vector<LabInfo>>> test_lab_data;
  return *test_lab_data;
}

void SetLabInfoForTesting(const std::vector<LabInfo>& test_feature_info) {
  if (test_feature_info.empty())
    GetTestData().reset();
  else
    GetTestData() = test_feature_info;
}

// TODO(elainechien): Explore better ways to allow developers to add their
// experiments.
// Experiments featured in labs must have feature entries of type FEATURE_VALUE
// (Default, Enabled, Disabled states). Experiments with multiple parameters may
// be considered in the future.
const std::vector<LabInfo>& GetData() {
  if (GetTestData())
    return GetTestData().value();

  static const base::NoDestructor<std::vector<LabInfo>> lab_info_([]() {
    std::vector<LabInfo> lab_info;

    // Read Later.
    lab_info.emplace_back(LabInfo(
        flag_descriptions::kReadLaterFlagId, base::ASCIIToUTF16("Reading List"),
        base::ASCIIToUTF16("Right click on a tab or click the Bookmark icon to "
                           "add tabs to a reading "
                           "list. Access from the Bookmarks bar."),
        version_info::Channel::BETA));

    // Tab Scrolling.
    lab_info.emplace_back(
        LabInfo(flag_descriptions::kScrollableTabStripFlagId,
                base::ASCIIToUTF16("Tab Scrolling"),
                base::ASCIIToUTF16(
                    "Enables tab strip to scroll left and right when full."),
                version_info::Channel::BETA));

    // Tab Search.
    lab_info.emplace_back(
        LabInfo(flag_descriptions::kEnableTabSearchFlagId,
                base::ASCIIToUTF16("Tab Search"),
                base::ASCIIToUTF16("Enable a popup bubble in Top Chrome UI to "
                                   "search over currently open tabs."),
                version_info::Channel::BETA));

    return lab_info;
  }());

  return *lab_info_;
}
}  // namespace

LabInfo::LabInfo(const std::string& internal_name,
                 const base::string16& visible_name,
                 const base::string16& visible_description,
                 version_info::Channel allowed_channel)
    : internal_name(internal_name),
      visible_name(visible_name),
      visible_description(visible_description),
      allowed_channel(allowed_channel) {}

LabInfo::LabInfo(const LabInfo& other) = default;

LabInfo::~LabInfo() = default;

ChromeLabsBubbleViewModel::ChromeLabsBubbleViewModel() : lab_info_(GetData()) {}

ChromeLabsBubbleViewModel::~ChromeLabsBubbleViewModel() = default;

const std::vector<LabInfo>& ChromeLabsBubbleViewModel::GetLabInfo() const {
  return lab_info_;
}

ScopedChromeLabsModelDataForTesting::ScopedChromeLabsModelDataForTesting() =
    default;

ScopedChromeLabsModelDataForTesting::~ScopedChromeLabsModelDataForTesting() {
  // Remove test data.
  SetLabInfoForTesting(std::vector<LabInfo>());  // IN-TEST
}

void ScopedChromeLabsModelDataForTesting::SetModelDataForTesting(
    const std::vector<LabInfo>& test_feature_info) {
  SetLabInfoForTesting(test_feature_info);  // IN-TEST
}
