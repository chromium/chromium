// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace omnibox_everywhere {

// Manages frameless fullscreen overlay widgets displaying desktop screenshots
// for region selection across all connected displays.
class OmniboxEverywhereRegionSelectOverlay : public views::WidgetObserver {
 public:
  using RegionCaptureSource = OmniboxEverywhereService::RegionCaptureSource;
  using CompleteCallback =
      base::OnceCallback<void(const SkBitmap& result_bitmap)>;

  static std::unique_ptr<OmniboxEverywhereRegionSelectOverlay> Create(
      const SkBitmap& screenshot,
      const RegionCaptureSource& source,
      CompleteCallback callback,
      gfx::NativeWindow context = gfx::NativeWindow());

  OmniboxEverywhereRegionSelectOverlay(
      const OmniboxEverywhereRegionSelectOverlay&) = delete;
  OmniboxEverywhereRegionSelectOverlay& operator=(
      const OmniboxEverywhereRegionSelectOverlay&) = delete;
  ~OmniboxEverywhereRegionSelectOverlay() override;

  // Returns the active / primary widget containing the cursor, or the first
  // widget if the cursor is outside all overlay widgets. For testing only.
  views::Widget* GetActiveWidgetForTesting();
  const views::Widget* GetActiveWidgetForTesting() const;

  const std::vector<std::unique_ptr<views::Widget>>& widgets_for_testing()
      const {
    return widgets_;
  }

  // Returns the sliced bitmap displayed on the widget at |widget_index| for
  // testing.
  const SkBitmap& GetBitmapForWidgetForTesting(size_t widget_index) const;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  explicit OmniboxEverywhereRegionSelectOverlay(CompleteCallback callback);
  void Initialize(const SkBitmap& screenshot,
                  const RegionCaptureSource& source,
                  gfx::NativeWindow context);

  std::unique_ptr<views::Widget> CreateWidgetForDisplay(
      const display::Display& display,
      const SkBitmap& display_bitmap,
      gfx::NativeWindow context);

  void Finish(const SkBitmap& result_bitmap);
  size_t GetActiveWidgetIndex() const;

  CompleteCallback callback_;
  std::vector<std::unique_ptr<views::Widget>> widgets_;
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
  base::WeakPtrFactory<OmniboxEverywhereRegionSelectOverlay> weak_factory_{
      this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_
