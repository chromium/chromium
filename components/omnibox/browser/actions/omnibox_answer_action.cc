// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_answer_action.h"

#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"
#endif

OmniboxAnswerAction::OmniboxAnswerAction(
    omnibox::SuggestionEnhancement enhancement,
    GURL destination_url)
    : OmniboxAction(
          OmniboxAction::LabelStrings(
              base::UTF8ToUTF16(enhancement.display_text()),
              base::UTF8ToUTF16(enhancement.display_text()),
              l10n_util::GetStringUTF16(
                  IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX),
              l10n_util::GetStringUTF16(IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST)),
          destination_url),
      enhancement_(std::move(enhancement)),
      destination_url_(destination_url) {}

OmniboxAnswerAction::~OmniboxAnswerAction() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
OmniboxAnswerAction::GetOrCreateJavaObject(JNIEnv* env) const {
  if (!j_omnibox_action_) {
    j_omnibox_action_.Reset(BuildOmniboxAnswerAction(
        env, reinterpret_cast<intptr_t>(this), strings_.hint,
        strings_.accessibility_hint, destination_url_));
  }
  return base::android::ScopedJavaLocalRef<jobject>(j_omnibox_action_);
}
#endif

OmniboxActionId OmniboxAnswerAction::ActionId() const {
  return OmniboxActionId::ANSWER_ACTION;
}

// static
const OmniboxAnswerAction* OmniboxAnswerAction::FromAction(
    const OmniboxAction* action) {
  return FromAction(const_cast<OmniboxAction*>(action));
}

// static
OmniboxAnswerAction* OmniboxAnswerAction::FromAction(OmniboxAction* action) {
  if (action && action->ActionId() == OmniboxActionId::ANSWER_ACTION) {
    return static_cast<OmniboxAnswerAction*>(action);
  }
  return nullptr;
}
