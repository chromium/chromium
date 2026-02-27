// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/live_toolbar_background.h"

#include <iterator>

#include "base/check.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace {
// Configuration for the mirror effect
constexpr float kBlurRadius = 15.0f;
constexpr int kGradientAlphaTop = 255;     // 0-255
constexpr int kGradientAlphaBottom = 100;  // 0-255

struct FramePinner {
  base::ReadOnlySharedMemoryMapping mapping;
  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      releaser;
};

void ReleaseFrame(void* addr, void* context) {
  delete static_cast<FramePinner*>(context);
}
}  // namespace

LiveToolbarBackground::LiveToolbarBackground(Browser* browser)
    : browser_(browser) {
  video_consumer_ = std::make_unique<VideoConsumer>(this);
  retry_timer_.Start(FROM_HERE, base::Seconds(1), this,
                     &LiveToolbarBackground::RetryStartCapture);

  if (browser_ && browser_->tab_strip_model()) {
    browser_->tab_strip_model()->AddObserver(this);
    // Start observing the initial tab
    content::WebContents* contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    if (contents) {
      Observe(contents);
    }
  }
  StartVideoCapture();
}

LiveToolbarBackground::~LiveToolbarBackground() {
  StopVideoCapture();
  if (browser_ && browser_->tab_strip_model()) {
    browser_->tab_strip_model()->RemoveObserver(this);
  }
}

void LiveToolbarBackground::RetryStartCapture() {
  if (!web_contents()) {
    // Try to attach to active tab if not yet observing
    content::WebContents* contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    if (contents) {
      Observe(contents);
    }
  }

  if (!is_capturing_video_ && web_contents()) {
    StartVideoCapture();
  }
}

void LiveToolbarBackground::Paint(gfx::Canvas* canvas,
                                  views::View* view) const {
  // Always fill with base color first to avoid artifacts/transparency issues
  SkColor toolbar_color = view->GetColorProvider()->GetColor(kColorToolbar);
  canvas->FillRect(view->GetLocalBounds(), toolbar_color);

  if (current_frame_.isNull()) {
    return;
  }

  // 1. Draw Mirrored Video with Blur and Gradient Mask
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(current_frame_);

  // We want to mirror the content.
  int src_w = std::min(image.width(), view->width());
  int src_h = std::min(image.height(), view->height());

  // We use a layer to apply the gradient mask AFTER the blur.
  // This ensures the opacity change is applied to the blurred result.
  canvas->SaveLayerAlpha(255, view->GetLocalBounds());

  canvas->Save();
  // Vertical Reflection: Translate to bottom edge, scale y by -1
  canvas->Translate(gfx::Vector2d(0, view->height()));
  canvas->Scale(1, -1);

  cc::PaintFlags image_flags;
  // Apply blur
  image_flags.setImageFilter(sk_make_sp<cc::BlurPaintFilter>(
      kBlurRadius, kBlurRadius, SkTileMode::kClamp, nullptr));

  // Draw the image at (0,0) in the flipped space.
  canvas->DrawImageInt(image, 0, 0, src_w, src_h, 0, 0, src_w, src_h, false,
                       image_flags);
  canvas->Restore();

  // 2. Apply Gradient Mask
  // We want to fade the blurred image out so the background toolbar color shows
  // through. To match the original behavior, we use the inverse of the previous
  // gradient alphas.
  SkColor4f colors[] = {
      SkColor4f::FromColor(SkColorSetA(SK_ColorBLACK, 255 - kGradientAlphaTop)),
      SkColor4f::FromColor(
          SkColorSetA(SK_ColorBLACK, 255 - kGradientAlphaBottom))};
  SkPoint points[] = {SkPoint::Make(0, 0), SkPoint::Make(0, view->height())};
  static_assert(std::size(colors) == std::size(points));

  cc::PaintFlags mask_flags;
  mask_flags.setShader(cc::PaintShader::MakeLinearGradient(
      points, colors, nullptr, std::size(points), SkTileMode::kClamp));
  mask_flags.setBlendMode(SkBlendMode::kDstIn);

  canvas->DrawRect(view->GetLocalBounds(), mask_flags);

  canvas->Restore();
}

void LiveToolbarBackground::SetView(views::View* view) {
  associated_view_ = view;
}

void LiveToolbarBackground::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    StopVideoCapture();

    // Switch observation to the new tab
    if (selection.new_contents) {
      Observe(selection.new_contents);
      StartVideoCapture();
    } else {
      Observe(nullptr);
    }
  }
}

void LiveToolbarBackground::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    // We rely on RenderFrameHostChanged to handle view changes if needed.
  }
}

void LiveToolbarBackground::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  StopVideoCapture();
}

void LiveToolbarBackground::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (new_host->IsInPrimaryMainFrame()) {
    StopVideoCapture();
    StartVideoCapture();
  }
}

void LiveToolbarBackground::StartVideoCapture() {
  if (is_capturing_video_) {
    return;
  }

  // Use the observed WebContents
  auto* contents = web_contents();
  if (!contents) {
    // Not observing yet?
    return;
  }

  auto* rwhv = contents->GetRenderWidgetHostView();
  if (!rwhv) {
    return;
  }

  video_capturer_ = rwhv->CreateVideoCapturer();
  if (!video_capturer_) {
    return;
  }

  gfx::Size target_size = rwhv->GetViewBounds().size();
  if (target_size.IsEmpty()) {
    target_size = gfx::Size(100, 100);
  }

  video_capturer_->SetResolutionConstraints(target_size, target_size, false);
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB);
  video_capturer_->SetMinCapturePeriod(base::Seconds(1) / 30);  // 30 FPS
  video_capturer_->Start(video_consumer_.get(),
                         viz::mojom::BufferFormatPreference::kDefault);

  is_capturing_video_ = true;
}

void LiveToolbarBackground::StopVideoCapture() {
  if (video_capturer_) {
    video_capturer_->Stop();
    video_capturer_.reset();
  }
  is_capturing_video_ = false;
  // Do NOT Stop observing WebContents here
}

void LiveToolbarBackground::OnFrameCaptured(SkBitmap bitmap) {
  current_frame_ = bitmap;
  if (associated_view_) {
    associated_view_->SchedulePaint();
  }
}

// VideoConsumer implementation
LiveToolbarBackground::VideoConsumer::VideoConsumer(
    LiveToolbarBackground* background)
    : background_(background) {}

LiveToolbarBackground::VideoConsumer::~VideoConsumer() = default;

void LiveToolbarBackground::VideoConsumer::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  if (!data->is_read_only_shmem_region()) {
    return;
  }

  base::ReadOnlySharedMemoryRegion& shmem_region =
      data->get_read_only_shmem_region();
  if (!shmem_region.IsValid()) {
    return;
  }

  base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
  if (!mapping.IsValid()) {
    return;
  }

  void* pixels = const_cast<void*>(mapping.memory());
  gfx::Size bitmap_size(content_rect.right(), content_rect.bottom());
  SkBitmap frame;

  FramePinner* pinner =
      new FramePinner{std::move(mapping), callbacks_remote.Unbind()};

  bool installed = frame.installPixels(
      SkImageInfo::MakeN32(bitmap_size.width(), bitmap_size.height(),
                           kPremul_SkAlphaType,
                           info->color_space.ToSkColorSpace()),
      pixels,
      media::VideoFrame::RowBytes(media::VideoFrame::Plane::kARGB,
                                  info->pixel_format, info->coded_size.width()),
      &ReleaseFrame, pinner);

  if (!installed) {
    delete pinner;
    return;
  }

  frame.setImmutable();

  SkBitmap cropped_frame;
  if (frame.extractSubset(&cropped_frame, gfx::RectToSkIRect(content_rect))) {
    background_->OnFrameCaptured(cropped_frame);
  } else {
    background_->OnFrameCaptured(frame);
  }
}
