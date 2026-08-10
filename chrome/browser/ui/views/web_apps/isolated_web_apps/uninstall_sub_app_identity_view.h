// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_UNINSTALL_SUB_APP_IDENTITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_UNINSTALL_SUB_APP_IDENTITY_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class ImageView;
}  // namespace views

class SkBitmap;

class UninstallSubAppIdentityView : public views::View {
  METADATA_HEADER(UninstallSubAppIdentityView, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kUninstallSubAppIdentityViewId);

  static std::unique_ptr<UninstallSubAppIdentityView> Create(
      const gfx::ImageSkia& image_skia,
      std::u16string app_title,
      std::u16string parent_app_title,
      bool should_mask_icon);

  ~UninstallSubAppIdentityView() override;

 private:
  UninstallSubAppIdentityView(const gfx::ImageSkia& image_skia,
                              std::u16string app_title,
                              std::u16string parent_app_title,
                              bool should_mask_icon);

  void OnIconMaskingComplete(SkBitmap masked_bitmap);

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  base::WeakPtrFactory<UninstallSubAppIdentityView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_UNINSTALL_SUB_APP_IDENTITY_VIEW_H_
