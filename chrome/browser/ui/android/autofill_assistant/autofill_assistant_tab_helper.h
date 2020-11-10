// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_ASSISTANT_AUTOFILL_ASSISTANT_TAB_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_ASSISTANT_AUTOFILL_ASSISTANT_TAB_HELPER_H_

#include "components/autofill_assistant/browser/controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill_assistant {

// AutofillAssistantTabHelper for the AutofillAssistant component.
class AutofillAssistantTabHelper
    : public content::WebContentsUserData<AutofillAssistantTabHelper>,
      public content::WebContentsObserver {
 public:
  ~AutofillAssistantTabHelper() override;
  AutofillAssistantTabHelper(const AutofillAssistantTabHelper&) = delete;
  AutofillAssistantTabHelper& operator=(const AutofillAssistantTabHelper&) =
      delete;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<AutofillAssistantTabHelper>;
  explicit AutofillAssistantTabHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_ASSISTANT_AUTOFILL_ASSISTANT_TAB_HELPER_H_
