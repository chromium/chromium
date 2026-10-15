// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_DOGFOOD_FEEDBACK_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_DOGFOOD_FEEDBACK_H_

#include <string>
#include <string_view>

class BrowserWindowInterface;

namespace omnibox_dogfood_feedback {

// Opens Chrome's feedback form in `browser`, for the dogfood feedback button
// in the Full WebUI omnibox.
void ShowFeedbackForm(BrowserWindowInterface* browser);

namespace internal {

// Returns whether `name` contains "omnibox" or "webui", ignoring case.
bool IsRelevantFeature(std::string_view name);

// Returns every relevant feature whose state was overridden (by Finch,
// chrome://flags or the command line), with its Finch trial, group and
// params.
std::string GetDiagnostics();

}  // namespace internal
}  // namespace omnibox_dogfood_feedback

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_DOGFOOD_FEEDBACK_H_
