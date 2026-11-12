// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_icon.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"
#endif

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

void OmniboxEverywhereWidgetDelegate::SetDraggableRegion(
    std::optional<SkRegion> region) {
  draggable_region_ = std::move(region);
}

bool OmniboxEverywhereWidgetDelegate::IsPointInDraggableRegion(
    const gfx::Point& point) const {
  return draggable_region_ && !draggable_region_->isEmpty() &&
         draggable_region_->contains(point.x(), point.y());
}

void OmniboxEverywhereWidgetDelegate::UpdateWindowIconForSearchEngine(
    const TemplateURLService* template_url_service) {
  window_icon_ = GetWindowIconFor(template_url_service);
  if (views::Widget* widget = GetWidget()) {
    widget->UpdateWindowIcon();
  }
}

int OmniboxEverywhereWidgetDelegate::NonClientHitTest(
    const gfx::Point& point) const {
#if BUILDFLAG(IS_MAC)
  // On MacOS, we rely on window dragging and therefore return HTCAPTION. We
  // avoid doing this on Aura because it causes the native context menu to
  // be shown when right-clicking, and we use custom event handling to implement
  // the window dragging. Ignore hover events so WebUI still receives mouse-move
  // events for `:hover` states inside draggable regions.
  if (IsPointInDraggableRegion(point) &&
      ShouldTreatCurrentEventAsDraggableOnMac()) {
    return HTCAPTION;
  }
#endif  // BUILDFLAG(IS_MAC)
  return HTNOWHERE;
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
