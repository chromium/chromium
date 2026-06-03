// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_

#include <memory>
#include <string>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "base/version.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class SkBitmap;

class WebAppIconNameAndOriginView : public views::View {
  METADATA_HEADER(WebAppIconNameAndOriginView, views::View)
 public:
  using StartUrl = base::StrongAlias<class StartUrlTag, GURL>;
  using ParentAppTitle =
      base::StrongAlias<class ParentAppTitleTag, std::u16string>;
  using AppInfo = std::variant<StartUrl, base::Version, ParentAppTitle>;

  static std::unique_ptr<WebAppIconNameAndOriginView> Create(
      const gfx::ImageSkia& image_skia,
      std::u16string app_title,
      const GURL& start_url,
      bool should_mask_icon);
  ~WebAppIconNameAndOriginView() override;

 protected:
  WebAppIconNameAndOriginView(const gfx::ImageSkia& image_skia,
                              std::u16string app_title,
                              AppInfo app_info,
                              bool should_mask_icon);

 private:
  void OnIconMaskingComplete(SkBitmap masked_bitmap);

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  base::WeakPtrFactory<WebAppIconNameAndOriginView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_ICON_NAME_AND_ORIGIN_VIEW_H_
