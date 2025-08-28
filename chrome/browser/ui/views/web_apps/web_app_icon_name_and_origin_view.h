// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class GURL;
class SkBitmap;

class WebAppIconNameAndOriginView : public views::View {
  METADATA_HEADER(WebAppIconNameAndOriginView, views::View)
 public:
  static std::unique_ptr<WebAppIconNameAndOriginView> Create(
      const gfx::ImageSkia& image_skia,
      std::u16string app_title,
      const GURL& start_url,
      bool should_mask_icon);
  ~WebAppIconNameAndOriginView() override;

 private:
  WebAppIconNameAndOriginView(const gfx::ImageSkia& image_skia,
                              std::u16string app_title,
                              const GURL& start_url,
                              bool should_mask_icon);

  void OnIconMaskingComplete(SkBitmap masked_bitmap);

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  base::WeakPtrFactory<WebAppIconNameAndOriginView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_
