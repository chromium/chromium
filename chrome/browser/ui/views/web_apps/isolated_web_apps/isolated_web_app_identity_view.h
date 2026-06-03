// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_IDENTITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_IDENTITY_VIEW_H_

#include <memory>
#include <string>

#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class IsolatedWebAppIdentityView : public WebAppIconNameAndOriginView {
  METADATA_HEADER(IsolatedWebAppIdentityView, WebAppIconNameAndOriginView)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIsolatedWebAppIdentityViewId);

  static std::unique_ptr<IsolatedWebAppIdentityView> Create(
      const gfx::ImageSkia& image_skia,
      std::u16string app_title,
      const web_app::IwaVersion& version,
      bool should_mask_icon);

  ~IsolatedWebAppIdentityView() override;

 private:
  IsolatedWebAppIdentityView(const gfx::ImageSkia& image_skia,
                             std::u16string app_title,
                             const web_app::IwaVersion& version,
                             bool should_mask_icon);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_IDENTITY_VIEW_H_
