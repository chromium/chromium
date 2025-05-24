// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace web_app {
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSimpleInstallDialogAppTitle);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSimpleInstallDialogIconView);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSimpleInstallDialogOriginLabel);
}  // namespace web_app

std::unique_ptr<WebAppIconNameAndOriginView>
WebAppIconNameAndOriginView::Create(const gfx::ImageSkia& icon_image,
                                    std::u16string app_title,
                                    const GURL& start_url) {
  return base::WrapUnique(
      new WebAppIconNameAndOriginView(icon_image, app_title, start_url));
}

WebAppIconNameAndOriginView::~WebAppIconNameAndOriginView() = default;

WebAppIconNameAndOriginView::WebAppIconNameAndOriginView(
    const gfx::ImageSkia& icon_image,
    std::u16string app_title,
    const GURL& start_url) {
  base::TrimWhitespace(app_title, base::TRIM_ALL, &app_title);
  int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      icon_label_spacing));

  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromImageSkia(icon_image));
  icon_view->SetProperty(views::kElementIdentifierKey,
                         web_app::kSimpleInstallDialogIconView);
  AddChildViewRaw(icon_view.release());

  views::View* labels = new views::View();
  AddChildViewRaw(labels);
  labels->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto name_label = web_app::CreateNameLabel(app_title);
  name_label->SetProperty(views::kElementIdentifierKey,
                          web_app::kSimpleInstallDialogAppTitle);
  labels->AddChildView(std::move(name_label));

  auto origin_label = web_app::CreateOriginLabelFromStartUrl(
      start_url, /*is_primary_text=*/false);
  origin_label->SetProperty(views::kElementIdentifierKey,
                            web_app::kSimpleInstallDialogOriginLabel);
  labels->AddChildView(std::move(origin_label));
}

BEGIN_METADATA(WebAppIconNameAndOriginView)
END_METADATA
