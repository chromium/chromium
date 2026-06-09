// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

class Profile;

namespace ntp_composebox {

// If overridden to false, disables the feature (kill switch). If true, enables
// the feature.
BASE_DECLARE_FEATURE(kNtpComposebox);

// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// Whether to show typed suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxTypedSuggest;
// Whether to show image suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxImageSuggestions;
// Whether to show the + entrypoint and contextual input menu in the realbox and
// composebox.
extern const base::FeatureParam<bool> kShowContextMenu;
// Whether to show tab previews on hover for the composebox context menu.
extern const base::FeatureParam<bool> kShowContextMenuTabPreviews;
// The maximum number of tab suggestions to show in the composebox context menu.
extern const base::FeatureParam<int> kContextMenuMaxTabSuggestions;
// Whether to enable multi-tab selection in the context menu.
extern const base::FeatureParam<bool> kContextMenuEnableMultiTabSelection;

// The maximum number of file attachments to upload.
extern const base::FeatureParam<int> kMaxNumFiles;
// Whether or not to show a description in the context menu entrypoint, or just
// the icon.
extern const base::FeatureParam<bool> kShowContextMenuDescription;
// Whether to show section headers in the context menu.
extern const base::FeatureParam<bool> kShowContextMenuHeaders;

// Whether to show the smart compose in the composebox.
extern const base::FeatureParam<bool> kShowSmartCompose;
// Whether to exit AI mode when the user clicks outside the composebox.
extern const base::FeatureParam<bool> kCloseComposeboxByClickOutside;
// Whether to show the AIM threads rail when composebox is open.
extern const base::FeatureParam<bool> kEnableThreadsRail;
// Whether to show the AIM threads rail Google logo.
extern const base::FeatureParam<bool> kEnableThreadsRailLogo;
// Whether to use ntp-composebox instead of cr-composebox.
extern const base::FeatureParam<bool> kUseNtpComposeboxFork;

bool IsNtpComposeboxEnabled(Profile* profile);

// Helper to create a QueryControllerConfigParams object from the feature
// params.
std::unique_ptr<
    contextual_search::ContextualSearchContextController::ConfigParams>
CreateQueryControllerConfigParams();

class FeatureConfig : public omnibox_feature_configs::Config<FeatureConfig> {
 public:
  // The configuration proto for the feature.
  omnibox::NTPComposeboxConfig config;

  FeatureConfig(const FeatureConfig&);
  FeatureConfig(FeatureConfig&&);
  FeatureConfig& operator=(const FeatureConfig&);
  FeatureConfig& operator=(FeatureConfig&&);
  ~FeatureConfig();

 private:
  friend class omnibox_feature_configs::Config<FeatureConfig>;
  friend class omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;
  FeatureConfig();
};

using ScopedFeatureConfigForTesting =
    omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;

}  // namespace ntp_composebox

namespace ntp_realbox {

// If overridden to false, disables the feature (kill switch). If true, enables
// the feature.
BASE_DECLARE_FEATURE(kNtpRealboxNext);

enum class PlaceholderText {
  ASK_OR_TYPE = 0,
  ASK = 1,
};

// The placeholder text to show on the search input.
extern const base::FeatureParam<PlaceholderText> kSteadyPlaceholder;

// Whether to show a series of cycling placeholder texts on the search input UI.
BASE_DECLARE_FEATURE(kNtpRealboxCyclingPlaceholders);

// Whether to enable multi-line input in the searchbox.
extern const base::FeatureParam<bool> kMultiLineEnabled;

// Whether to enable the dynamic version of the AI Mode button in the realbox.
BASE_DECLARE_FEATURE(kNtpRealboxDynamicAiModeButton);

bool IsNtpRealboxNextEnabled(Profile* profile);

}  // namespace ntp_realbox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
