// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_BLUR_LAYER_DELEGATE_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_BLUR_LAYER_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_context.h"

namespace lens {

// LayerDelegate for controlling the background blur behind the overlay. This
// class is only a LayerDelegate to control the layer painting and does not own
// the layer.
class LensOverlayBlurLayerDelegate : public ui::LayerOwner,
                                     public ui::LayerDelegate,
                                     public content::RenderWidgetHostObserver {
 public:
  // Creates a layer and starts painting to it with a blurred background image
  // using the given render view as the background. Note: Initializing this
  // class does not start capturing screenshots of the background view until
  // StartBackgroundImageCapture(). Meaning, until StartBackgroundImageCapture,
  // this layer will paint a static blurred image that was taken on
  // initialization.
  explicit LensOverlayBlurLayerDelegate(
      content::RenderWidgetHost* background_view_host);
  ~LensOverlayBlurLayerDelegate() override;

  // Starts taking screenshots of the background view to use for blurring.
  void StartBackgroundImageCapture();

  // Pauses taking screenshots of the background view. When paused, the layer
  // will still be blurred, but it will be a static blur that does not update.
  // When the page resizes, it will stretch the last taken screenshot to fit the
  // new layer size.
  void StopBackgroundImageCapture();

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // Fetches a new background screenshot to use for blurring.
  void FetchBackgroundImage();

  // Updates background_screenshot_ to the new bitmap and rerenders IFF bitmap
  // is visually different than background_screenshot_.
  void UpdateBackgroundImage(const SkBitmap& bitmap);

  // The latest screenshot being used to render the background.
  SkBitmap background_screenshot_;

  // A timer to periodically take screenshots of the underlying page.
  base::RepeatingTimer screenshot_timer_;

  // Pointer to the RenderWidgetHost to get the contents which we are
  // blurring. Owned by the live page web contents, so is possible to become
  // null.
  raw_ptr<content::RenderWidgetHost> background_view_host_;

  // Observes the RenderWidgetHost to ensure our pointer stays valid.
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      render_widget_host_observer_{this};

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayBlurLayerDelegate> weak_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_BLUR_LAYER_DELEGATE_H_
