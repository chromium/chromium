// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_DOGFOOD_FEEDBACK_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_DOGFOOD_FEEDBACK_H_

#include <string>
#include <string_view>

#include "chrome/browser/feedback/public/feedback_source.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace omnibox_dogfood_feedback {

// Opens Chrome's feedback form in `browser`, for the dogfood feedback button
// in the Full WebUI omnibox.
void ShowFeedbackForm(BrowserWindowInterface* browser);

// Opens the same form as above, for dogfood feedback buttons on other
// surfaces, with `page_url`, `description_header` (placed at the top of the
// description template), `source`, and `category_tag`.
void ShowFeedbackForm(Profile* profile,
                      const GURL& page_url,
                      std::string_view description_header,
                      feedback::FeedbackSource source,
                      const std::string& category_tag);

namespace internal {

// Returns whether `name` contains "omnibox", "webui", or "contextual",
// ignoring case.
bool IsRelevantFeature(std::string_view name);

// Returns every relevant feature whose state was overridden (by Finch,
// chrome://flags or the command line), with its Finch trial, group and
// params.
std::string GetDiagnostics();

}  // namespace internal
}  // namespace omnibox_dogfood_feedback

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_DOGFOOD_FEEDBACK_H_
