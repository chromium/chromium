// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill_assistant/autofill_assistant_tab_helper.h"

#include "base/time/default_tick_clock.h"

namespace autofill_assistant {

AutofillAssistantTabHelper::~AutofillAssistantTabHelper() {}

void AutofillAssistantTabHelper::WebContentsDestroyed() {}

AutofillAssistantTabHelper::AutofillAssistantTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillAssistantTabHelper)

}  // namespace autofill_assistant
