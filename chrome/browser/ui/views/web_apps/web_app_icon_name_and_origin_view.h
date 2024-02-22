// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_

#include <memory>
#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class GURL;

class WebAppIconNameAndOriginView : public views::View {
  METADATA_HEADER(WebAppIconNameAndOriginView, views::View)
 public:
  static std::unique_ptr<WebAppIconNameAndOriginView> Create(
      const gfx::ImageSkia& image_skia,
      std::u16string app_title,
      const GURL& start_url);
  ~WebAppIconNameAndOriginView() override;

 private:
  WebAppIconNameAndOriginView(const gfx::ImageSkia& image_skia,
                              std::u16string app_title,
                              const GURL& start_url);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_
