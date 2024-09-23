// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ANSWER_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ANSWER_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"

// OmniboxAnswerAction is an action associated with a match for answer
// verticals.
class OmniboxAnswerAction : public OmniboxAction {
 public:
  OmniboxAnswerAction(omnibox::SuggestionEnhancement enhancement,
                      TemplateURLRef::SearchTermsArgs search_terms_args,
                      omnibox::AnswerType answer_type);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif

  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;
  static const OmniboxAnswerAction* FromAction(const OmniboxAction* action);
  static OmniboxAnswerAction* FromAction(OmniboxAction* action);

  TemplateURLRef::SearchTermsArgs search_terms_args;

 private:
  ~OmniboxAnswerAction() override;

  omnibox::SuggestionEnhancement enhancement_;
  omnibox::AnswerType answer_type_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ANSWER_ACTION_H_
