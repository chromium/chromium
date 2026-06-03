// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_identity_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IsolatedWebAppIdentityView,
                                      kIsolatedWebAppIdentityViewId);

std::unique_ptr<IsolatedWebAppIdentityView> IsolatedWebAppIdentityView::Create(
    const gfx::ImageSkia& icon_image,
    std::u16string app_title,
    const web_app::IwaVersion& version,
    bool should_mask_icon) {
  return base::WrapUnique(new IsolatedWebAppIdentityView(
      icon_image, std::move(app_title), version, should_mask_icon));
}

IsolatedWebAppIdentityView::~IsolatedWebAppIdentityView() = default;

IsolatedWebAppIdentityView::IsolatedWebAppIdentityView(
    const gfx::ImageSkia& image_skia,
    std::u16string app_title,
    const web_app::IwaVersion& version,
    bool should_mask_icon)
    : WebAppIconNameAndOriginView(image_skia,
                                  std::move(app_title),
                                  version.version(),
                                  should_mask_icon) {
  SetProperty(views::kElementIdentifierKey, kIsolatedWebAppIdentityViewId);
}

BEGIN_METADATA(IsolatedWebAppIdentityView)
END_METADATA
