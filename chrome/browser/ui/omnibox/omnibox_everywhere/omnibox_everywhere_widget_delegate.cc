// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_icon.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

namespace {

ui::ImageModel GetWindowIconFor(
    const TemplateURLService* template_url_service) {
  return ui::ImageModel::FromImageSkia(
      GetOmniboxEverywhereIcon(template_url_service,
                               /*for_dark_background=*/false));
}

}  // namespace

OmniboxEverywhereWidgetDelegate::OmniboxEverywhereWidgetDelegate(
    const TemplateURLService* template_url_service)
    : window_icon_(GetWindowIconFor(template_url_service)) {
  SetCanActivate(true);
  SetHasWindowSizeControls(false);
}

OmniboxEverywhereWidgetDelegate::~OmniboxEverywhereWidgetDelegate() = default;

void OmniboxEverywhereWidgetDelegate::UpdateWindowIconForSearchEngine(
    const TemplateURLService* template_url_service) {
  window_icon_ = GetWindowIconFor(template_url_service);
  if (views::Widget* widget = GetWidget()) {
    widget->UpdateWindowIcon();
  }
}

ui::ImageModel OmniboxEverywhereWidgetDelegate::GetWindowIcon() {
  return window_icon_;
}

ui::ImageModel OmniboxEverywhereWidgetDelegate::GetWindowAppIcon() {
  return GetWindowIcon();
}

std::u16string OmniboxEverywhereWidgetDelegate::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_NAME);
}

}  // namespace omnibox_everywhere
