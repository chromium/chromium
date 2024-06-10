// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ANSWER_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ANSWER_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"

// OmniboxAnswerAction is an action associated with a match for answer
// verticals.
class OmniboxAnswerAction : public OmniboxAction {
 public:
  OmniboxAnswerAction(omnibox::SuggestionEnhancement enhancement,
                      GURL destination_url);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif

  OmniboxActionId ActionId() const override;
  static const OmniboxAnswerAction* FromAction(const OmniboxAction* action);
  static OmniboxAnswerAction* FromAction(OmniboxAction* action);

 private:
  ~OmniboxAnswerAction() override;

  omnibox::SuggestionEnhancement enhancement_;
  GURL destination_url_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ANSWER_ACTION_H_
