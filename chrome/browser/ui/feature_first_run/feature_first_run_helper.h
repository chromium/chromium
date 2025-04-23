// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_
#define CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

class RichControlsContainerView;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace feature_first_run {

enum class InfoBoxPosition { kStart, kMiddle, kEnd };

// Show a tab-modal dialog from the supplied params on the `web_contents`.
views::Widget* ShowFeatureFirstRunDialog(
    std::u16string title,
    ui::ImageModel banner,
    ui::ImageModel dark_mode_banner,
    std::unique_ptr<views::View> content_view,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    content::WebContents* web_contents);

// Creates a styled infobox container view an icon, title and description.
std::unique_ptr<RichControlsContainerView> CreateInfoBoxContainer(
    std::u16string title,
    std::u16string description,
    const gfx::VectorIcon& vector_icon,
    InfoBoxPosition position);

// Creates a styled infobox container view an icon, title and a description with
// a learn more link.
std::unique_ptr<RichControlsContainerView> CreateInfoBoxContainerWithLearnMore(
    std::u16string title,
    int description_id,
    const std::u16string& learn_more,
    base::RepeatingClosure learn_more_callback,
    const gfx::VectorIcon& vector_icon,
    InfoBoxPosition position);

// Creates an empty container view to wrap the dialog content.
std::unique_ptr<views::View> CreateDialogContentViewContainer();

}  // namespace feature_first_run

#endif  // CHROME_BROWSER_UI_FEATURE_FIRST_RUN_FEATURE_FIRST_RUN_HELPER_H_
