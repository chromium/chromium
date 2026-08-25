// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace omnibox_everywhere {

// Manages a frameless fullscreen overlay widget displaying a desktop
// screenshot for region selection.
class OmniboxEverywhereRegionSelectOverlay : public views::WidgetObserver {
 public:
  using CompleteCallback =
      base::OnceCallback<void(const SkBitmap& result_bitmap)>;

  static std::unique_ptr<OmniboxEverywhereRegionSelectOverlay> Create(
      const SkBitmap& screenshot,
      CompleteCallback callback,
      gfx::NativeWindow context = gfx::NativeWindow());

  OmniboxEverywhereRegionSelectOverlay(
      const OmniboxEverywhereRegionSelectOverlay&) = delete;
  OmniboxEverywhereRegionSelectOverlay& operator=(
      const OmniboxEverywhereRegionSelectOverlay&) = delete;
  ~OmniboxEverywhereRegionSelectOverlay() override;

  views::Widget* widget() { return widget_.get(); }
  const views::Widget* widget() const { return widget_.get(); }

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  explicit OmniboxEverywhereRegionSelectOverlay(CompleteCallback callback);
  void Initialize(const SkBitmap& screenshot, gfx::NativeWindow context);
  void Finish(const SkBitmap& result_bitmap);

  CompleteCallback callback_;
  std::unique_ptr<views::Widget> widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_
