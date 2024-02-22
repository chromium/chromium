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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "url/gurl.h"

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
  AddChildView(icon_view.release());

  views::View* labels = new views::View();
  AddChildView(labels);
  labels->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  labels->AddChildView(web_app::CreateNameLabel(app_title).release());
  labels->AddChildView(
      web_app::CreateOriginLabelFromStartUrl(start_url, false).release());
}

BEGIN_METADATA(WebAppIconNameAndOriginView)
END_METADATA
