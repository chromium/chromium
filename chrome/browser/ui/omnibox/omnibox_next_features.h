// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

class Profile;

namespace omnibox {

// The `internal` namespace contains implementation details for omnibox
// features. It is exposed only for use by about_flags.cc and in unit tests.
//
// DO NOT USE THESE FEATURE FLAGS DIRECTLY FROM OTHER CODE.
// Instead, use the helper functions defined below (e.g., `IsAimPopupEnabled`).
namespace internal {

BASE_DECLARE_FEATURE(kWebUIOmniboxPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxAimPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxSimplification);
BASE_DECLARE_FEATURE(kWebUIOmniboxAskGAboutThisPage);

}  // namespace internal

enum class AddContextButtonVariant {
  // Variant 1.
  kBelowResults = 1,
  // Variant 2.
  kInline = 2,
};

extern const base::FeatureParam<AddContextButtonVariant>
    kWebUIOmniboxAimPopupAddContextButtonVariantParam;
extern const base::FeatureParam<bool> kHideClassicContextButton;
BASE_DECLARE_FEATURE(kAiModeEntryPointAlwaysNavigates);
BASE_DECLARE_FEATURE(kAiModeSpaceDoesNotActivate);
BASE_DECLARE_FEATURE(kWebUIOmniboxDisableCaretColorAnimation);
BASE_DECLARE_FEATURE(kWebUIOmniboxAimPopupDisableAnimation);
BASE_DECLARE_FEATURE(kWebUIOmniboxFullPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxFullPopupV2);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopupDebug);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopupSelectionControl);
// Caret animation for omnibox
BASE_DECLARE_FEATURE(kOmniboxAnimatedCaret);
// Enables energy effect in the omnibox.
BASE_DECLARE_FEATURE(kEnergyEffectInOmnibox);
BASE_DECLARE_FEATURE(kWebUIOmniboxDynamicAiModeButton);

extern const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam;

// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// Whether to enable multi-tab selection in the context menu.
extern const base::FeatureParam<bool> kContextMenuEnableMultiTabSelection;
// The maximum number of tab suggestions to show in the composebox context menu.
extern const base::FeatureParam<int> kContextMenuMaxTabSuggestions;
// Whether to show image suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxImageSuggestions;
// Whether to show typed suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxTypedSuggest;
// Whether to show zps suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxZps;
// Whether to show the + entrypoint and contextual input menu in the realbox and
// composebox.
extern const base::FeatureParam<bool> kShowContextMenu;
// Whether or not to show a description in the context menu entrypoint, or just
// the icon.
// TODO (crbug.com/509939902): Remove this when finch experiment reference
// is removed.
extern const base::FeatureParam<bool> kShowContextMenuDescription;
// Whether to show tab previews on hover for the composebox context menu.
extern const base::FeatureParam<bool> kShowContextMenuTabPreviews;
// Whether to show the lens search chip in the composebox.
extern const base::FeatureParam<bool> kShowLensSearchChip;
// Whether to show the smart compose in the composebox.
extern const base::FeatureParam<bool> kShowSmartCompose;
// Whether to show the tools and models in the composebox.
extern const base::FeatureParam<bool> kShowToolsAndModels;
// Whether to show section headers in the context menu.
extern const base::FeatureParam<bool> kShowContextMenuHeaders;
// Whether to use the composebox fork.
extern const base::FeatureParam<bool> kUseComposeboxFork;
// Whether to use the grey oblong background for context menu entrypoint.
extern const base::FeatureParam<bool> kContextButtonHasBackground;
// Whether the button should be an oblong shape vs circular.
extern const base::FeatureParam<bool> kContextButtonShapeIsOblong;
// Whether to show the "Ask about tabs" label for the context menu entrypoint.
extern const base::FeatureParam<bool> kContextButtonShowSuggestionLabel;
// If enabled, then the WebUI Omnibox will be rendered in a WebView in the
// BrowserView.
extern const base::FeatureParam<bool> kWebUIOmniboxFullPopupV2UseBrowserView;
// Whether to open the next panel with cobrowse.
extern const base::FeatureParam<bool> kAskGCoBrowse;
// Whether to open the next panel with cobrowse and visual selection.
extern const base::FeatureParam<bool> kAskGCoBrowseWithVisualSelection;

// Returns true if `kWebUIOmniboxPopup` is enabled.
bool IsWebUIOmniboxPopupEnabled();

// Returns true if either `kWebUIOmniboxFullPopup` or `kWebUIOmniboxFullPopupV2`
// is enabled.
bool IsWebUIOmniboxFullPopupEnabled();

// Returns true if `kWebUIOmniboxInBrowserView` is enabled.
bool IsWebUIOmniboxInBrowserViewEnabled();

// Returns true if the `kWebUIOmniboxAimPopup` base::Feature is enabled.
// This does NOT include user eligibility checks. Most UI code should use the
// profile-based `IsAimPopupEnabled()` function below instead.
bool IsAimPopupFeatureEnabled();

// Returns true if the AIM Popup feature is fully enabled for the given
// `profile`. This is the correct function for external code to use, as it
// checks both the base::Feature flag and all other requirements like user
// eligibility.
bool IsAimPopupEnabled(Profile* profile);
bool ShouldShowAimContextMenuOption(Profile* profile);

// Returns true if search content sharing is permitted by enterprise policy.
bool IsContentSharingEnabled(
    Profile* profile,
    contextual_search::ContextualSearchSessionHandle* session_handle);

bool IsCreateImagesEnabled(Profile* profile);
bool IsDeepSearchEnabled(Profile* profile);

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

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
