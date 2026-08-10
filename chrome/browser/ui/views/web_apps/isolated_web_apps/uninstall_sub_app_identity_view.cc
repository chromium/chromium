// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/uninstall_sub_app_identity_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/icons/icon_masker.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::Label> CreateSubAppUninstallInfoLabel(
    const std::u16string& parent_app_name,
    const std::u16string& sub_app_name) {
  auto uninstall_info_label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_IWA_SUB_APPS_UNINSTALL_INFO,
                                 parent_app_name, sub_app_name),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY);
  uninstall_info_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  uninstall_info_label->SetMultiLine(true);
  return uninstall_info_label;
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(UninstallSubAppIdentityView,
                                      kUninstallSubAppIdentityViewId);

std::unique_ptr<UninstallSubAppIdentityView>
UninstallSubAppIdentityView::Create(const gfx::ImageSkia& icon_image,
                                    std::u16string app_title,
                                    std::u16string parent_app_title,
                                    bool should_mask_icon) {
  return base::WrapUnique(new UninstallSubAppIdentityView(
      icon_image, std::move(app_title), std::move(parent_app_title),
      should_mask_icon));
}

UninstallSubAppIdentityView::~UninstallSubAppIdentityView() = default;

UninstallSubAppIdentityView::UninstallSubAppIdentityView(
    const gfx::ImageSkia& icon_image,
    std::u16string app_title,
    std::u16string parent_app_title,
    bool should_mask_icon) {
  SetProperty(views::kElementIdentifierKey, kUninstallSubAppIdentityViewId);

  base::TrimWhitespace(app_title, base::TRIM_ALL, &app_title);
  int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      icon_label_spacing));

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  if (should_mask_icon) {
    web_app::MaskIconOnOs(
        *icon_image.bitmap(),
        base::BindOnce(&UninstallSubAppIdentityView::OnIconMaskingComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  icon_view_->SetImage(ui::ImageModel::FromImageSkia(icon_image));
  icon_view_->SetProperty(views::kElementIdentifierKey,
                          web_app::kSimpleInstallDialogIconView);

  views::View* labels_ptr = AddChildView(std::make_unique<views::View>());
  labels_ptr->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto name_label = web_app::CreateNameLabel(app_title);
  name_label->SetProperty(views::kElementIdentifierKey,
                          web_app::kSimpleInstallDialogAppTitle);
  labels_ptr->AddChildView(std::move(name_label));

  auto app_info_label =
      CreateSubAppUninstallInfoLabel(parent_app_title, app_title);
  app_info_label->SetProperty(views::kElementIdentifierKey,
                              web_app::kSimpleInstallDialogAppInfoLabel);
  labels_ptr->AddChildView(std::move(app_info_label));
}

void UninstallSubAppIdentityView::OnIconMaskingComplete(
    SkBitmap masked_bitmap) {
  CHECK(icon_view_);
  gfx::Image masked_image = gfx::Image::CreateFrom1xBitmap(masked_bitmap);
  icon_view_->SetImage(ui::ImageModel::FromImage(masked_image));
}

BEGIN_METADATA(UninstallSubAppIdentityView)
END_METADATA
