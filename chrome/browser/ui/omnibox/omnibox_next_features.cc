// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace {
constexpr base::FeatureState DISABLED = base::FEATURE_DISABLED_BY_DEFAULT;
// constexpr base::FeatureState ENABLED = base::FEATURE_ENABLED_BY_DEFAULT;
}  // namespace

namespace omnibox {

constexpr base::FeatureParam<AddContextButtonVariant>::Option
    kAddContextButtonVariantOptions[] = {
        {AddContextButtonVariant::kNone, "none"},
        {AddContextButtonVariant::kBelowResults, "below_results"},
        {AddContextButtonVariant::kAboveResults, "above_results"},
        {AddContextButtonVariant::kInline, "inline"}};

// If enabled, Omnibox popup will transition to AI-Mode with the compose-box
// panel taking up the whole of the popup, covering the location bar completely.
BASE_FEATURE(kWebUIOmniboxAimPopup, DISABLED);
// Configures the placement of the "Add Context" button in the Omnibox popup.
const base::FeatureParam<AddContextButtonVariant>
    kWebUIOmniboxAimPopupAddContextButtonVariantParam{
        &kWebUIOmniboxAimPopup, "AddContextButtonVariant",
        AddContextButtonVariant::kNone, &kAddContextButtonVariantOptions};
// If enabled, removes the cutout for the location bar and fills the entire
// popup content with the WebUI WebView.
BASE_FEATURE(kWebUIOmniboxFullPopup, DISABLED);
// If enabled, shows the omnibox suggestions in the popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup, DISABLED);
// Enables the WebUI for omnibox suggestions without modifying the popup UI.
BASE_FEATURE(kWebUIOmniboxPopupDebug, DISABLED);
// Enables side-by-side comparison omnibox suggestions in WebUI and Views.
const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam{
    &kWebUIOmniboxPopupDebug, "SxS", false};

}  // namespace omnibox
