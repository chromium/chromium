// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_IN_SUGGEST_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_IN_SUGGEST_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

class OmniboxActionInSuggest : public OmniboxAction {
 public:
  OmniboxActionInSuggest(
      omnibox::SuggestTemplateInfo::TemplateAction template_action,
      std::optional<TemplateURLRef::SearchTermsArgs> search_terms_args);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif

  void RecordActionShown(size_t position, bool used) const override;
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;

  omnibox::SuggestTemplateInfo_TemplateAction_ActionType Type() const;

  // Downcasts the given OmniboxAction to an OmniboxActionInSuggest if the
  // supplied instance represents one, otherwise returns nullptr.
  static const OmniboxActionInSuggest* FromAction(const OmniboxAction* action);
  static OmniboxActionInSuggest* FromAction(OmniboxAction* action);
  // Static function that registers that an action with a specified type was
  // shown or used. This function can be employed when avoiding the use of the
  // action cpp pointer.
  static void RecordShownAndUsedMetrics(
      omnibox::SuggestTemplateInfo_TemplateAction_ActionType type,
      bool used);

  omnibox::SuggestTemplateInfo::TemplateAction template_action;
  std::optional<TemplateURLRef::SearchTermsArgs> search_terms_args;
  int tab_id = 0;

 protected:
  FRIEND_TEST_ALL_PREFIXES(OmniboxActionInSuggestTest, ShowAsActionButton);
  FRIEND_TEST_ALL_PREFIXES(OmniboxActionInSuggestTest,
                           ShowAsActionButtonForTabSwitch);

 private:
  ~OmniboxActionInSuggest() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_IN_SUGGEST_H_
