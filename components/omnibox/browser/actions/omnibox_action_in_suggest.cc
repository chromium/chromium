// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/actions/omnibox_pedal_jni_wrapper.h"
#include "url/android/gurl_android.h"
#endif

namespace {
// UMA reported Type of ActionInSuggest.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox.action
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ActionInSuggestUmaType
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The values should remain
// synchronized with the enum AutocompleteMatchType in
// //tools/metrics/histograms/enums.xml.
enum class ActionInSuggestUmaType {
  kUnknown = 0,
  kCall,
  kDirections,
  kWebsite,

  // Sentinel value. Must be set to the last valid ActionInSuggestUmaType.
  kMaxValue = kWebsite
};

// Get the UMA action type from ActionInfo::ActionType.
constexpr ActionInSuggestUmaType ToUmaActionType(
    omnibox::ActionInfo_ActionType action_type) {
  switch (action_type) {
    case omnibox::ActionInfo_ActionType_CALL:
      return ActionInSuggestUmaType::kCall;

    case omnibox::ActionInfo_ActionType_DIRECTIONS:
      return ActionInSuggestUmaType::kDirections;

    case omnibox::ActionInfo_ActionType_WEBSITE:
      return ActionInSuggestUmaType::kWebsite;

    default:
      return ActionInSuggestUmaType::kUnknown;
  }
}

}  // namespace

OmniboxActionInSuggest::OmniboxActionInSuggest(omnibox::ActionInfo action_info)
    : OmniboxAction(
          OmniboxAction::LabelStrings(
              base::UTF8ToUTF16(action_info.displayed_text()),
              base::UTF8ToUTF16(action_info.displayed_text()),
              l10n_util::GetStringUTF16(
                  IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX),
              l10n_util::GetStringUTF16(IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST)),
          {},
          false),
      action_info_{std::move(action_info)} {}

OmniboxActionInSuggest::~OmniboxActionInSuggest() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
OmniboxActionInSuggest::GetOrCreateJavaObject(JNIEnv* env) const {
  if (!j_omnibox_action_) {
    std::string serialized_action;
    if (!action_info_.SerializeToString(&serialized_action)) {
      serialized_action.clear();
    }
    j_omnibox_action_.Reset(
        BuildOmniboxActionInSuggest(env, strings_.hint, serialized_action));
  }
  return base::android::ScopedJavaLocalRef<jobject>(j_omnibox_action_);
}
#endif

void OmniboxActionInSuggest::RecordActionShown(size_t position,
                                               bool executed) const {
  base::UmaHistogramEnumeration("Omnibox.ActionInSuggest.Shown",
                                ToUmaActionType(action_info_.action_type()));
  if (executed) {
    base::UmaHistogramEnumeration("Omnibox.ActionInSuggest.Used",
                                  ToUmaActionType(action_info_.action_type()));
  }
}

void OmniboxActionInSuggest::Execute(ExecutionContext& context) const {
  // Note: this is platform-dependent.
  // There's currently no code wiring ActionInSuggest on the Desktop and iOS.
  // TODO(crbug/1418077): log searchboxstats metrics.
  NOTREACHED() << "Not implemented";
}

OmniboxActionId OmniboxActionInSuggest::ActionId() const {
  return OmniboxActionId::ACTION_IN_SUGGEST;
}
