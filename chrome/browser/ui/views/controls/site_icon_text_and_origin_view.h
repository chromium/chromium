// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_SITE_ICON_TEXT_AND_ORIGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_SITE_ICON_TEXT_AND_ORIGIN_VIEW_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

// This is a simple reusable view that has an |icon| on the left with an
// empty text field on the right, filled with the |title|. The |url|
// appears under the text field. Used by the DIY web app install dialog, where
// the name of the app is configurable from the view.
// *------------------------------------------------*
// | Image | Text Field                             |
// |________________________________________________|
// |         Url                                    |
// |________________________________________________|
// *-------------------------------------------------*
// Based on current usages of the dialog, an empty text field will disable the
// Ok button, whose behavior is handled here.
class SiteIconTextAndOriginView : public views::View,
                                  public views::TextfieldController {
  METADATA_HEADER(SiteIconTextAndOriginView, views::View)

 public:
  SiteIconTextAndOriginView(const gfx::ImageSkia& icon,
                            std::u16string initial_title,
                            std::u16string accessible_title,
                            const GURL& url,
                            content::WebContents* web_contents,
                            base::RepeatingCallback<void(const std::u16string&)>
                                text_tracker_callback);

  ~SiteIconTextAndOriginView() override;

  views::Textfield* title_field() { return title_field_; }

 protected:
  // views::TextfieldController override
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  raw_ptr<views::Textfield> title_field_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  base::RepeatingCallback<void(const std::u16string&)> text_tracker_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_SITE_ICON_TEXT_AND_ORIGIN_VIEW_H_
