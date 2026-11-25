// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

class RegionSelectOverlayView;

// Manages frameless fullscreen overlay widgets displaying desktop screenshots
// for region selection across all connected displays.
class OmniboxEverywhereRegionSelectOverlay {
 public:
  using RegionCaptureSource = OmniboxEverywhereService::RegionCaptureSource;
  using CompleteCallback =
      base::OnceCallback<void(const SkBitmap& result_bitmap)>;
  // Maps a display id to a screenshot of that display.
  using DisplayScreenshots = base::flat_map<int64_t, SkBitmap>;

  static std::unique_ptr<OmniboxEverywhereRegionSelectOverlay> Create(
      const SkBitmap& screenshot,
      const RegionCaptureSource& source,
      CompleteCallback callback,
      gfx::NativeWindow context = gfx::NativeWindow());

  // Like `Create()`, but takes one screenshot per display rather than one
  // screenshot that spans the displays. Shows a widget on every connected
  // display that has a non-empty screenshot, and ignores the entries for
  // displays that are not connected. The underlying pixels of each used bitmap
  // are marked immutable. A selection cannot extend beyond the display it
  // starts on, so the result is always cropped from a single screenshot.
  static std::unique_ptr<OmniboxEverywhereRegionSelectOverlay>
  CreateForDisplays(const DisplayScreenshots& screenshots,
                    CompleteCallback callback,
                    gfx::NativeWindow context = gfx::NativeWindow());

  OmniboxEverywhereRegionSelectOverlay(
      const OmniboxEverywhereRegionSelectOverlay&) = delete;
  OmniboxEverywhereRegionSelectOverlay& operator=(
      const OmniboxEverywhereRegionSelectOverlay&) = delete;
  ~OmniboxEverywhereRegionSelectOverlay();

  // Returns the active / primary widget containing the cursor, or the first
  // widget if the cursor is outside all overlay widgets. For testing only.
  views::Widget* GetActiveWidgetForTesting();
  const views::Widget* GetActiveWidgetForTesting() const;

  const std::vector<std::unique_ptr<views::Widget>>& widgets_for_testing()
      const {
    return widgets_;
  }

  // Returns the sliced bitmap displayed on the widget at `widget_index` for
  // testing.
  const SkBitmap& GetBitmapForWidgetForTesting(size_t widget_index) const;

 private:
  friend class RegionSelectOverlayView;

  struct DisplaySliceInfo {
    display::Display display;
    // The screenshot that shows `display`. Slices share one screenshot when it
    // spans several displays.
    SkBitmap screenshot;
    // The part of `screenshot` that shows `display`, in pixels.
    gfx::Rect sub_rect_in_screenshot;
  };

  OmniboxEverywhereRegionSelectOverlay(CompleteCallback callback,
                                       bool confine_selection_to_display);

  // Set as close callback on every overlay widget via
  // Widget::MakeCloseSynchronous(). Runs synchronously when a widget is closed
  // by anything other than the overlay itself (Close()/CloseWithReason(),
  // CloseNow(), or the platform destroying the native window) and cancels the
  // whole selection in unison.
  void OnWidgetCloseRequested(views::Widget::ClosedReason reason);

  // Drag coordination methods called by RegionSelectOverlayView:
  void OnDragStarted();
  void OnDragUpdated(int64_t source_display_id,
                     const gfx::Rect& global_selection_rect);
  void OnDragCompleted(int64_t source_display_id,
                       const gfx::Rect& global_selection_rect);
  void OnDragCancelled();
  bool confine_selection_to_display() const {
    return confine_selection_to_display_;
  }

  void Initialize(const SkBitmap& screenshot,
                  const RegionCaptureSource& source,
                  gfx::NativeWindow context);
  void InitializeForDisplays(const DisplayScreenshots& screenshots,
                             gfx::NativeWindow context);

  // Posts `Finish(SkBitmap())` so cancellation runs asynchronously.
  void PostEmptyFinish();

  // Populates `widgets_` with one fullscreen overlay widget per entry in
  // `display_slices_`, shows them, and activates the widget nearest the cursor.
  // Requires `display_slices_` to be non-empty.
  void CreateWidgets(gfx::NativeWindow context);

  std::unique_ptr<views::Widget> CreateWidgetForDisplay(
      const display::Display& display,
      const SkBitmap& display_bitmap,
      gfx::NativeWindow context);

  void Finish(const SkBitmap& result_bitmap);
  void ResetWidgets();
  // Returns `global_selection_rect` clipped to the display with
  // `source_display_id` if selections are confined to the display they start
  // on. Returns it unchanged otherwise.
  gfx::Rect MaybeConfineSelection(int64_t source_display_id,
                                  const gfx::Rect& global_selection_rect) const;
  SkBitmap CropGlobalSelection(int64_t source_display_id,
                               const gfx::Rect& global_selection_rect) const;
  size_t GetActiveWidgetIndex() const;

  CompleteCallback callback_;
  // Whether a selection is limited to the display that it starts on.
  const bool confine_selection_to_display_;
  std::vector<DisplaySliceInfo> display_slices_;
  std::vector<std::unique_ptr<views::Widget>> widgets_;
  base::WeakPtrFactory<OmniboxEverywhereRegionSelectOverlay> weak_factory_{
      this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_REGION_SELECT_OVERLAY_H_
