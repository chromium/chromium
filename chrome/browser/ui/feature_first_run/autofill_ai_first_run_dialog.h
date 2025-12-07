// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FEATURE_FIRST_RUN_AUTOFILL_AI_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_FEATURE_FIRST_RUN_AUTOFILL_AI_FIRST_RUN_DIALOG_H_

namespace content {
class WebContents;
}  // namespace content

namespace feature_first_run {

// Shows a tab-modal dialog which displays the Autofill AI opt-in flow.
void ShowAutofillAiFirstRunDialog(content::WebContents* web_contents);

}  // namespace feature_first_run

#endif  // CHROME_BROWSER_UI_FEATURE_FIRST_RUN_AUTOFILL_AI_FIRST_RUN_DIALOG_H_
