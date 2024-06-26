// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_answer_action.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"
#endif

namespace {
constexpr const char* ToUmaUsageHistogramName(
    SuggestionAnswer::AnswerType answer_type) {
  switch (answer_type) {
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
      return "Omnibox.AnswerAction.UsageByType.Dictionary";
    case SuggestionAnswer::ANSWER_TYPE_FINANCE:
      return "Omnibox.AnswerAction.UsageByType.Finance";
    case SuggestionAnswer::ANSWER_TYPE_KNOWLEDGE_GRAPH:
      return "Omnibox.AnswerAction.UsageByType.KnowledgeGraph";
    case SuggestionAnswer::ANSWER_TYPE_LOCAL:
      return "Omnibox.AnswerAction.UsageByType.Local";
    case SuggestionAnswer::ANSWER_TYPE_SPORTS:
      return "Omnibox.AnswerAction.UsageByType.Sports";
    case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
      return "Omnibox.AnswerAction.UsageByType.Sunrise";
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return "Omnibox.AnswerAction.UsageByType.Translation";
    case SuggestionAnswer::ANSWER_TYPE_WEATHER:
      return "Omnibox.AnswerAction.UsageByType.Weather";
    case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
      return "Omnibox.AnswerAction.UsageByType.WhenIs";
    case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
      return "Omnibox.AnswerAction.UsageByType.Currency";
    case SuggestionAnswer::ANSWER_TYPE_LOCAL_TIME:
      return "Omnibox.AnswerAction.UsageByType.LocalTime";
    case SuggestionAnswer::ANSWER_TYPE_PLAY_INSTALL:
      return "Omnibox.AnswerAction.UsageByType.PlayInstall";
    case SuggestionAnswer::ANSWER_TYPE_INVALID:
    default:
      return "Omnibox.AnswerAction.UsageByType.Invalid";
  }
}

}  // namespace

OmniboxAnswerAction::OmniboxAnswerAction(
    omnibox::SuggestionEnhancement enhancement,
    TemplateURLRef::SearchTermsArgs search_terms_args,
    SuggestionAnswer::AnswerType answer_type)
    : OmniboxAction(
          OmniboxAction::LabelStrings(
              base::UTF8ToUTF16(enhancement.display_text()),
              base::UTF8ToUTF16(enhancement.display_text()),
              l10n_util::GetStringUTF16(
                  IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX),
              l10n_util::GetStringUTF16(IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST)),
          {}),
      search_terms_args(search_terms_args),
      enhancement_(std::move(enhancement)),
      answer_type_(answer_type) {}

OmniboxAnswerAction::~OmniboxAnswerAction() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
OmniboxAnswerAction::GetOrCreateJavaObject(JNIEnv* env) const {
  if (!j_omnibox_action_) {
    j_omnibox_action_.Reset(
        BuildOmniboxAnswerAction(env, reinterpret_cast<intptr_t>(this),
                                 strings_.hint, strings_.accessibility_hint));
  }
  return base::android::ScopedJavaLocalRef<jobject>(j_omnibox_action_);
}
#endif

void OmniboxAnswerAction::RecordActionShown(size_t position,
                                            bool executed) const {
  base::UmaHistogramEnumeration(
      "Omnibox.AnswerAction.Shown", answer_type_,
      SuggestionAnswer::AnswerType::ANSWER_TYPE_TOTAL_COUNT);
  if (executed) {
    base::UmaHistogramEnumeration(
        "Omnibox.AnswerAction.Used", answer_type_,
        SuggestionAnswer::AnswerType::ANSWER_TYPE_TOTAL_COUNT);
  }

  base::UmaHistogramBoolean(ToUmaUsageHistogramName(answer_type_), executed);
}

void OmniboxAnswerAction::Execute(ExecutionContext& context) const {}

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
