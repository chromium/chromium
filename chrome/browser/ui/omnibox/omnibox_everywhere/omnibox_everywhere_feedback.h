// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEEDBACK_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEEDBACK_H_

class Profile;

namespace omnibox_everywhere {

// Returns true when the dogfood feedback menu option should be shown for
// `profile`. Returns false if `profile` is `nullptr`.
bool ShouldShowFeedback(const Profile* profile);

// Opens Chrome's feedback form for `profile` (which must be non-null)
// pre-populated with the Omnibox Everywhere dogfood bug template and
// Omnibox/WebUI diagnostics.
void ShowFeedbackForm(Profile* profile);

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEEDBACK_H_
