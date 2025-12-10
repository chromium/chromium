// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/contextual_search_context_controller.h"
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

BASE_DECLARE_FEATURE(kWebUIOmniboxAimPopup);

}  // namespace internal

enum class AddContextButtonVariant {
  // No "Add Context" button.
  kNone = 0,
  // Variant 1.
  kBelowResults = 1,
  // Variant 2.
  kAboveResults = 2,
  // Variant 3.
  kInline = 3,
};

extern const base::FeatureParam<AddContextButtonVariant>
    kWebUIOmniboxAimPopupAddContextButtonVariantParam;
BASE_DECLARE_FEATURE(kWebUIOmniboxFullPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopupDebug);
extern const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam;

// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// Whether to exit AI mode when the user clicks outside the composebox.
extern const base::FeatureParam<bool> kCloseComposeboxByClickOutside;
// Whether to exit AI mode when the user clicks Escape in the composebox.
extern const base::FeatureParam<bool> kCloseComposeboxByEscape;
// Whether to enable multi-tab selection in the context menu.
extern const base::FeatureParam<bool> kContextMenuEnableMultiTabSelection;
// The maximum number of tab suggestions to show in the composebox context menu.
extern const base::FeatureParam<int> kContextMenuMaxTabSuggestions;
// Whether or not to enable viewport images with page context uploads.
extern const base::FeatureParam<bool> kEnableViewportImages;
// Whether to force tools and models to show in the composebox context menu.
extern const base::FeatureParam<bool> kForceToolsAndModels;
// The maximum number of file attachments to upload.
extern const base::FeatureParam<int> kMaxNumFiles;
// Whether to send the lns_surface parameter.
// TODO(crbug.com/430070871): Remove this flag once the server supports the
// `lns_surface` parameter.
extern const base::FeatureParam<bool> kSendLnsSurfaceParam;
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
extern const base::FeatureParam<bool> kShowContextMenuDescription;
// Whether to show tab previews on hover for the composebox context menu.
extern const base::FeatureParam<bool> kShowContextMenuTabPreviews;
// Whether to show the create image button in the composebox context menu.
extern const base::FeatureParam<bool> kShowCreateImageTool;
// Whether to show the lens search chip in the composebox.
extern const base::FeatureParam<bool> kShowLensSearchChip;
// Whether to show the recent tab chip in the composebox.
extern const base::FeatureParam<bool> kShowRecentTabChip;
// Whether to show the smart compose in the composebox.
extern const base::FeatureParam<bool> kShowSmartCompose;
// Whether to show the submit button in the composebox.
extern const base::FeatureParam<bool> kShowSubmit;
// Whether to show the tools and models picker in the composebox.
extern const base::FeatureParam<bool> kShowToolsAndModels;
// Whether to show the voice search button in steady state composebox.
extern const base::FeatureParam<bool> kShowVoiceSearchInSteadyComposebox;
// Whether to show the voice search button in expanded composebox.
extern const base::FeatureParam<bool> kShowVoiceSearchInExpandedComposebox;
// If kSendLnsSurfaceParam is true, whether to suppress the `lns_surface`
// parameter if there is no image upload. Does nothing if kSendLnsSurfaceParam
// is false.
extern const base::FeatureParam<bool> kSuppressLnsSurfaceParamIfNoImage;
// Whether or not to use separate request ids for viewport images if the
// multi-context input flow is enabled.
extern const base::FeatureParam<bool>
    kUseSeparateRequestIdsForMultiContextViewportImages;

// Returns true if the `kWebUIOmniboxAimPopup` base::Feature is enabled.
// This does NOT include user eligibility checks. Most UI code should use the
// profile-based `IsAimPopupEnabled()` function below instead.
bool IsAimPopupFeatureEnabled();

// Returns true if the AIM Popup feature is fully enabled for the given
// `profile`. This is the correct function for external code to use, as it
// checks both the base::Feature flag and all other requirements like user
// eligibility.
bool IsAimPopupEnabled(Profile* profile);

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
