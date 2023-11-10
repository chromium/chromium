// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs_model.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

absl::optional<std::vector<LabInfo>>& GetTestData() {
  static base::NoDestructor<absl::optional<std::vector<LabInfo>>> test_lab_data;
  return *test_lab_data;
}

void SetLabInfoForTesting(const std::vector<LabInfo>& test_feature_info) {
  if (test_feature_info.empty()) {
    GetTestData().reset();
  } else {
    GetTestData() = test_feature_info;
  }
}

// TODO(elainechien): Explore better ways to allow developers to add their
// experiments.
// Experiments featured in labs must have feature entries of type FEATURE_VALUE
// (Default, Enabled, Disabled states) or FEATURE_WITH_PARAMS_VALUE
const std::vector<LabInfo>& GetData() {
  if (GetTestData()) {
    return GetTestData().value();
  }

  static const base::NoDestructor<std::vector<LabInfo>> lab_info_([]() {
    std::vector<LabInfo> lab_info;

    // Tab Groups Save.
    lab_info.emplace_back(
        flag_descriptions::kTabGroupsSaveId,
        l10n_util::GetStringUTF16(IDS_TAB_GROUPS_SAVE_EXPERIMENT_NAME),
        l10n_util::GetStringUTF16(IDS_TAB_GROUPS_SAVE_DESCRIPTION),
        "tab-groups-save", version_info::Channel::BETA);

    // CustomizeChromeSidePanel.
    lab_info.emplace_back(
        flag_descriptions::kCustomizeChromeSidePanelId,
        l10n_util::GetStringUTF16(
            IDS_CUSTOMIZE_CHROME_SIDE_PANEL_EXPERIMENT_NAME),
        l10n_util::GetStringUTF16(IDS_CUSTOMIZE_CHROME_SIDE_PANEL_DESCRIPTION),
        "chrome-labs-customize-chrome-side-panel", version_info::Channel::BETA);

    // ChromeRefresh2023.
    std::vector<std::u16string> chrome_refresh_variation_descriptions = {
        l10n_util::GetStringUTF16(IDS_CHROMEREFRESH2023_WITHOUT_OMNIBOX)};

    lab_info.emplace_back(
        flag_descriptions::kChromeRefresh2023Id,
        l10n_util::GetStringUTF16(IDS_CHROMEREFRESH2023_EXPERIMENT_NAME),
        l10n_util::GetStringUTF16(IDS_CHROMEREFRESH2023_DESCRIPTION),
        "chrome-refresh", version_info::Channel::BETA,
        chrome_refresh_variation_descriptions);

    // ChromeWebuiRefresh2023.
    lab_info.emplace_back(
        flag_descriptions::kChromeWebuiRefresh2023Id,
        l10n_util::GetStringUTF16(IDS_CHROMEWEBUIREFRESH2023_EXPERIMENT_NAME),
        l10n_util::GetStringUTF16(IDS_CHROMEWEBUIREFRESH2023_DESCRIPTION),
        "chrome-labs-webui-refresh", version_info::Channel::BETA);

    // Tab Scrolling.
    std::vector<std::u16string> tab_scrolling_variation_descriptions = {
        l10n_util::GetStringUTF16(IDS_TABS_SHRINK_TO_PINNED_TAB_WIDTH),
        l10n_util::GetStringUTF16(IDS_TABS_SHRINK_TO_MEDIUM_WIDTH),
        l10n_util::GetStringUTF16(IDS_TABS_SHRINK_TO_LARGE_WIDTH),
        l10n_util::GetStringUTF16(IDS_TABS_DO_NOT_SHRINK)};

    lab_info.emplace_back(
        flag_descriptions::kScrollableTabStripFlagId,
        l10n_util::GetStringUTF16(IDS_TAB_SCROLLING_EXPERIMENT_NAME),
        l10n_util::GetStringUTF16(IDS_TAB_SCROLLING_EXPERIMENT_DESCRIPTION),
        "chrome-labs-tab-scrolling", version_info::Channel::BETA,
        tab_scrolling_variation_descriptions);

    // Thumbnail Tab Strip for Windows.
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH))
    lab_info.emplace_back(
        flag_descriptions::kWebUITabStripFlagId,
        l10n_util::GetStringUTF16(IDS_THUMBNAIL_TAB_STRIP_EXPERIMENT_NAME),
        l10n_util::GetStringUTF16(
            IDS_THUMBNAIL_TAB_STRIP_EXPERIMENT_DESCRIPTION),
        "chrome-labs-thumbnail-tab-strip", version_info::Channel::BETA);
#endif

    return lab_info;
  }());

  return *lab_info_;
}
}  // namespace

LabInfo::LabInfo(
    const std::string& internal_name,
    const std::u16string& visible_name,
    const std::u16string& visible_description,
    const std::string& feedback_category_name,
    version_info::Channel allowed_channel,
    std::vector<std::u16string> translated_feature_variation_descriptions)
    : internal_name(internal_name),
      visible_name(visible_name),
      visible_description(visible_description),
      feedback_category_name(feedback_category_name),
      allowed_channel(allowed_channel),
      translated_feature_variation_descriptions(
          translated_feature_variation_descriptions) {}

LabInfo::LabInfo(const LabInfo& other) = default;

LabInfo::~LabInfo() = default;

ChromeLabsModel::ChromeLabsModel() : lab_info_(GetData()) {}

ChromeLabsModel::~ChromeLabsModel() = default;

const std::vector<LabInfo>& ChromeLabsModel::GetLabInfo() const {
  return *lab_info_;
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
