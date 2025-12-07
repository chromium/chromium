// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_update_identity_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace web_app {

WebAppUpdateIdentityView::WebAppUpdateIdentityView(
    const WebAppIdentity& identity) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto* icon_view = AddChildView(std::make_unique<views::ImageView>());
  icon_view->SetImageSize(
      gfx::Size(kIconSizeForUpdateDialog, kIconSizeForUpdateDialog));
  icon_view->SetImage(ui::ImageModel::FromImage(identity.icon));
  icon_view->SetProperty(views::kElementIdentifierKey,
                         WebAppUpdateIdentityView::kIconLabelId);

  auto* name_label = AddChildView(web_app::CreateNameLabel(identity.title));
  name_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  name_label->SetProperty(views::kElementIdentifierKey,
                          WebAppUpdateIdentityView::kNameLabelId);

  auto* origin_label = AddChildView(web_app::CreateOriginLabelFromStartUrl(
      identity.start_url, /*is_primary_text=*/false));
  origin_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

WebAppUpdateIdentityView::~WebAppUpdateIdentityView() = default;

BEGIN_METADATA(WebAppUpdateIdentityView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppUpdateIdentityView, kIconLabelId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppUpdateIdentityView, kNameLabelId);

}  // namespace web_app
