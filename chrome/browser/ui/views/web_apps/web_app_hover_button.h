// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_HOVER_BUTTON_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"
#include "url/gurl.h"

namespace ui {
class Event;
}

namespace web_app {
class WebAppProvider;
}  // namespace web_app

// WebAppHoverButton is a hoverable button with a primary left-hand icon, a
// title and a subtitle.
class WebAppHoverButton : public HoverButton {
 public:
  METADATA_HEADER(WebAppHoverButton);

  // Creates a hoverable button with the given elements, like so:
  //
  // +-------------------------------------------------------------------+
  // |      |    title                                                   |
  // | icon |                                                            |
  // |      |    subtitle                                                |
  // +-------------------------------------------------------------------+
  //
  WebAppHoverButton(views::Button::PressedCallback callback,
                    const web_app::AppId& app_id,
                    web_app::WebAppProvider* provider,
                    const std::u16string& display_name,
                    const GURL& url);
  WebAppHoverButton(views::Button::PressedCallback callback,
                    const gfx::ImageSkia& icon,
                    const std::u16string& display_name);
  WebAppHoverButton(const WebAppHoverButton&) = delete;
  WebAppHoverButton& operator=(const WebAppHoverButton&) = delete;
  ~WebAppHoverButton() override;

  virtual void MarkAsUnselected(const ui::Event* event);
  virtual void MarkAsSelected(const ui::Event* event);

  const web_app::AppId& app_id() const { return app_id_; }

 private:
  void OnIconsRead(std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  const web_app::AppId app_id_;
  base::WeakPtrFactory<WebAppHoverButton> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(, WebAppHoverButton, HoverButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, WebAppHoverButton)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_HOVER_BUTTON_H_
