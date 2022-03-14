// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_hover_button.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "url/gurl.h"
#include "url/origin.h"

WebAppHoverButton::WebAppHoverButton(views::Button::PressedCallback callback,
                                     const web_app::AppId& app_id,
                                     web_app::WebAppProvider* provider,
                                     const std::u16string& display_name,
                                     const GURL& url)
    : HoverButton(std::move(callback),
                  std::make_unique<NonAccessibleImageView>(),
                  display_name,
                  url_formatter::FormatUrlForSecurityDisplay(
                      url,
                      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
                  /*secondary_view=*/nullptr,
                  /*resize_row_for_secondary_view=*/false,
                  /*secondary_view_can_process_events=*/false),
      app_id_(app_id) {
  provider->icon_manager().ReadIcons(
      app_id_, IconPurpose::ANY,
      provider->registrar().GetAppDownloadedIconSizesAny(app_id_),
      base::BindOnce(&WebAppHoverButton::OnIconsRead,
                     weak_ptr_factory_.GetWeakPtr()));

  title()->SetDefaultTextStyle(STYLE_EMPHASIZED);
  Layout();
}

WebAppHoverButton::WebAppHoverButton(views::Button::PressedCallback callback,
                                     const gfx::ImageSkia& icon,
                                     const std::u16string& display_name)
    : HoverButton(std::move(callback),
                  ui::ImageModel::FromImageSkia(icon),
                  display_name) {
  Layout();
}

WebAppHoverButton::~WebAppHoverButton() = default;

void WebAppHoverButton::MarkAsUnselected(const ui::Event* event) {
  views::InkDrop::Get(this)->AnimateToState(
      views::InkDropState::HIDDEN, ui::LocatedEvent::FromIfValid(event));
}

void WebAppHoverButton::MarkAsSelected(const ui::Event* event) {
  views::InkDrop::Get(this)->AnimateToState(
      views::InkDropState::ACTIVATED, ui::LocatedEvent::FromIfValid(event));
}

void WebAppHoverButton::OnIconsRead(
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty())
    return;

  gfx::Size image_size{web_app::kWebAppIconSmall, web_app::kWebAppIconSmall};
  auto imageSkia =
      gfx::ImageSkia(std::make_unique<WebAppInfoImageSource>(
                         web_app::kWebAppIconSmall, std::move(icon_bitmaps)),
                     image_size);
  SetImage(views::Button::ButtonState::STATE_NORMAL, imageSkia);
}

BEGIN_METADATA(WebAppHoverButton, HoverButton)
END_METADATA
