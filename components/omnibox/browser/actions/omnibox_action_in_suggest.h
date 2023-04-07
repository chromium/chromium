// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_IN_SUGGEST_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_IN_SUGGEST_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/omnibox_proto/entity_info.pb.h"

class OmniboxActionInSuggest : public OmniboxAction {
 public:
  explicit OmniboxActionInSuggest(omnibox::ActionInfo action_info);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif

  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;

 private:
  ~OmniboxActionInSuggest() override;

  omnibox::ActionInfo action_info_{};
#if BUILDFLAG(IS_ANDROID)
  mutable base::android::ScopedJavaGlobalRef<jobject> j_omnibox_action_;
#endif
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_IN_SUGGEST_H_
