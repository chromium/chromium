// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_SUB_APP_IDENTITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_SUB_APP_IDENTITY_VIEW_H_

#include <memory>
#include <string>

#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class SubAppIdentityView : public WebAppIconNameAndOriginView {
  METADATA_HEADER(SubAppIdentityView, WebAppIconNameAndOriginView)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSubAppIdentityViewId);

  static std::unique_ptr<SubAppIdentityView> Create(
      const gfx::ImageSkia& image_skia,
      std::u16string app_title,
      std::u16string parent_app_title,
      bool should_mask_icon);

  ~SubAppIdentityView() override;

 private:
  SubAppIdentityView(const gfx::ImageSkia& image_skia,
                     std::u16string app_title,
                     std::u16string parent_app_title,
                     bool should_mask_icon);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_SUB_APP_IDENTITY_VIEW_H_
