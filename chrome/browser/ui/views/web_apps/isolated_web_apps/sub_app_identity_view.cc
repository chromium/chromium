// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/sub_app_identity_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SubAppIdentityView,
                                      kSubAppIdentityViewId);

std::unique_ptr<SubAppIdentityView> SubAppIdentityView::Create(
    const gfx::ImageSkia& icon_image,
    std::u16string app_title,
    std::u16string parent_app_title,
    bool should_mask_icon) {
  return base::WrapUnique(
      new SubAppIdentityView(icon_image, std::move(app_title),
                             std::move(parent_app_title), should_mask_icon));
}

SubAppIdentityView::~SubAppIdentityView() = default;

SubAppIdentityView::SubAppIdentityView(const gfx::ImageSkia& image_skia,
                                       std::u16string app_title,
                                       std::u16string parent_app_title,
                                       bool should_mask_icon)
    : WebAppIconNameAndOriginView(image_skia,
                                  std::move(app_title),
                                  WebAppIconNameAndOriginView::ParentAppTitle(
                                      std::move(parent_app_title)),
                                  should_mask_icon) {
  SetProperty(views::kElementIdentifierKey, kSubAppIdentityViewId);
}

BEGIN_METADATA(SubAppIdentityView)
END_METADATA
