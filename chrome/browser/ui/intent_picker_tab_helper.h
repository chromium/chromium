// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Controls the visibility of IntentPickerView by updating the visibility based
// on stored state.
class IntentPickerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<IntentPickerTabHelper> {
 public:
  ~IntentPickerTabHelper() override;

  static void SetShouldShowIcon(content::WebContents* web_contents,
                                bool should_show_icon);

  bool should_show_icon() const { return should_show_icon_; }

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  explicit IntentPickerTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<IntentPickerTabHelper>;

  bool should_show_icon_ = false;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerTabHelper);
};

#endif  // CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
