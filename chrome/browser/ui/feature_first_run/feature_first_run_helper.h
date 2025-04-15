// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_
#define CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_

#include <string>

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

namespace feature_first_run {

// Show a tab-modal dialog from the supplied params on the `web_contents`.
// TODO(crbug.com/409520456): Add banner, content view and button callbacks as
// params to construct the dialog.
views::Widget* ShowFeatureFirstRunDialog(std::u16string title,
                                         content::WebContents* web_contents);

}  // namespace feature_first_run

#endif  // CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_
