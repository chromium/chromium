// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_
#define CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_

#include <memory>
#include <string>

class RichControlsContainerView;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace feature_first_run {

enum class InfoBoxPosition { kStart, kMiddle, kEnd };

// Show a tab-modal dialog from the supplied params on the `web_contents`.
// TODO(crbug.com/409520456): Add banner, content view and button callbacks as
// params to construct the dialog.
views::Widget* ShowFeatureFirstRunDialog(std::u16string title,
                                         content::WebContents* web_contents);

std::unique_ptr<RichControlsContainerView> CreateInfoBoxContainer(
    const std::u16string& title,
    const std::u16string& description,
    const gfx::VectorIcon& vector_icon,
    InfoBoxPosition position);

}  // namespace feature_first_run

#endif  // CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_
