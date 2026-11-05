// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WIDGET_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WIDGET_DELEGATE_H_

#include <optional>

#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget_delegate.h"

class TemplateURLService;

namespace omnibox_everywhere {

// Defines the custom window behavior and layout for the desktop Omnibox
// Everywhere widget.
class OmniboxEverywhereWidgetDelegate : public views::WidgetDelegate {
 public:
  // `template_url_service` selects the window icon (see
  // `GetOmniboxEverywhereIcon()`). It may be null and is not retained.
  explicit OmniboxEverywhereWidgetDelegate(
      const TemplateURLService* template_url_service);
  OmniboxEverywhereWidgetDelegate(const OmniboxEverywhereWidgetDelegate&) =
      delete;
  OmniboxEverywhereWidgetDelegate& operator=(
      const OmniboxEverywhereWidgetDelegate&) = delete;
  ~OmniboxEverywhereWidgetDelegate() override;

  void SetDraggableRegion(std::optional<SkRegion> region);
  bool IsPointInDraggableRegion(const gfx::Point& point) const;

  // Recomputes the window icon for `template_url_service` (see
  // `GetOmniboxEverywhereIcon()`) and applies it to the widget, if any.
  void UpdateWindowIconForSearchEngine(
      const TemplateURLService* template_url_service);

  int NonClientHitTest(const gfx::Point& point) const;

  // views::WidgetDelegate:
  ui::ImageModel GetWindowIcon() override;
  ui::ImageModel GetWindowAppIcon() override;
  std::u16string GetWindowTitle() const override;

 private:
  ui::ImageModel window_icon_;
  std::optional<SkRegion> draggable_region_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WIDGET_DELEGATE_H_
