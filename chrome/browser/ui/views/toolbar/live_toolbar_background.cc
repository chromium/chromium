// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/live_toolbar_background.h"

#include <iterator>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
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
#include "ui/base/class_property.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(BrowserLiveBackgroundController*)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(BrowserLiveBackgroundController,
                                   kBrowserLiveBackgroundControllerKey)

namespace {
// Configuration for the mirror effect
constexpr float kBlurRadius = 30.0f;
constexpr int kGradientAlphaTop = 220;     // 0-255
constexpr int kGradientAlphaBottom = 220;  // 0-255

struct FramePinner {
  base::ReadOnlySharedMemoryMapping mapping;
  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      releaser;
};

void ReleaseFrame(void* addr, void* context) {
  delete static_cast<FramePinner*>(context);
}
}  // namespace

BrowserLiveBackgroundController::BrowserLiveBackgroundController(
    BrowserView* browser_view)
    : browser_(browser_view->browser()) {
  video_consumer_ = std::make_unique<VideoConsumer>(this);
  retry_timer_.Start(FROM_HERE, base::Seconds(1), this,
                     &BrowserLiveBackgroundController::RetryStartCapture);

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

BrowserLiveBackgroundController::~BrowserLiveBackgroundController() {
  StopVideoCapture();
  if (browser_ && browser_->tab_strip_model()) {
    browser_->tab_strip_model()->RemoveObserver(this);
  }
}

// static
BrowserLiveBackgroundController* BrowserLiveBackgroundController::GetOrCreate(
    BrowserView* browser_view) {
  if (!browser_view) {
    return nullptr;
  }
  auto* controller =
      browser_view->GetProperty(kBrowserLiveBackgroundControllerKey);
  if (!controller) {
    auto new_controller =
        std::make_unique<BrowserLiveBackgroundController>(browser_view);
    controller = browser_view->SetProperty(kBrowserLiveBackgroundControllerKey,
                                           std::move(new_controller));
  }
  return controller;
}

void BrowserLiveBackgroundController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BrowserLiveBackgroundController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserLiveBackgroundController::RetryStartCapture() {
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

void BrowserLiveBackgroundController::OnTabStripModelChanged(
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

void BrowserLiveBackgroundController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    // We rely on RenderFrameHostChanged to handle view changes if needed.
  }
}

void BrowserLiveBackgroundController::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  StopVideoCapture();
}

void BrowserLiveBackgroundController::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (new_host->IsInPrimaryMainFrame()) {
    StopVideoCapture();
    StartVideoCapture();
  }
}

void BrowserLiveBackgroundController::StartVideoCapture() {
  if (is_capturing_video_) {
    return;
  }

  auto* contents = web_contents();
  if (!contents) {
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

  video_capturer_->SetResolutionConstraints(gfx::Size(10, 10),
                                            gfx::Size(10000, 10000), false);
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB);
  video_capturer_->SetMinCapturePeriod(base::Seconds(1) / 30);  // 30 FPS
  video_capturer_->Start(video_consumer_.get(),
                         viz::mojom::BufferFormatPreference::kDefault);

  is_capturing_video_ = true;
}

void BrowserLiveBackgroundController::StopVideoCapture() {
  if (video_capturer_) {
    video_capturer_->Stop();
    video_capturer_.reset();
  }
  is_capturing_video_ = false;
}

void BrowserLiveBackgroundController::OnFrameCaptured(SkBitmap bitmap) {
  current_frame_ = bitmap;
  for (auto& observer : observers_) {
    observer.OnFrameCaptured();
  }
}

// VideoConsumer implementation
BrowserLiveBackgroundController::VideoConsumer::VideoConsumer(
    BrowserLiveBackgroundController* controller)
    : controller_(controller) {}

BrowserLiveBackgroundController::VideoConsumer::~VideoConsumer() = default;

void BrowserLiveBackgroundController::VideoConsumer::OnFrameCaptured(
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
    controller_->OnFrameCaptured(cropped_frame);
  } else {
    controller_->OnFrameCaptured(frame);
  }
}

// LiveToolbarBackground implementation
LiveToolbarBackground::LiveToolbarBackground(BrowserView* browser_view,
                                             views::View* view)
    : controller_(BrowserLiveBackgroundController::GetOrCreate(browser_view)),
      view_(view) {
  if (controller_) {
    controller_->AddObserver(this);
  }
}

LiveToolbarBackground::~LiveToolbarBackground() {
  if (controller_) {
    controller_->RemoveObserver(this);
  }
}

void LiveToolbarBackground::OnFrameCaptured() {
  if (view_) {
    view_->SchedulePaint();
  }
}

void LiveToolbarBackground::Paint(gfx::Canvas* canvas,
                                  views::View* view) const {
  SkColor toolbar_color = view->GetColorProvider()->GetColor(kColorToolbar);

  // Base background
  canvas->FillRect(view->GetLocalBounds(), toolbar_color);

  if (!controller_ || controller_->current_frame().isNull()) {
    return;
  }

  // Layer for the gradient mask
  canvas->SaveLayerAlpha(255, view->GetLocalBounds());

  // Layer for the blur
  cc::PaintFlags blur_flags;
  blur_flags.setImageFilter(sk_make_sp<cc::BlurPaintFilter>(
      kBlurRadius, kBlurRadius, SkTileMode::kClamp, nullptr));
  canvas->SaveLayerWithFlags(blur_flags);

  // Fill toolbar color in the blur layer so video edges blend into it
  canvas->FillRect(view->GetLocalBounds(), toolbar_color);

  gfx::ImageSkia image =
      gfx::ImageSkia::CreateFrom1xBitmap(controller_->current_frame());

  gfx::Point view_origin_in_screen = view->GetBoundsInScreen().origin();
  gfx::Rect wc_bounds_in_screen = view->GetBoundsInScreen();
  content::WebContents* web_contents = controller_->active_web_contents();
  if (web_contents && web_contents->GetRenderWidgetHostView()) {
    wc_bounds_in_screen =
        web_contents->GetRenderWidgetHostView()->GetViewBounds();
  }

  int x_offset = wc_bounds_in_screen.x() - view_origin_in_screen.x();
  int y_offset = wc_bounds_in_screen.y() - view_origin_in_screen.y();

  canvas->Save();
  // Vertical Reflection: Translate to the top of the web contents, scale y by
  // -1
  canvas->Translate(gfx::Vector2d(x_offset, y_offset));
  canvas->Scale(1, -1);

  cc::PaintFlags image_flags;
  // Draw the image scaled to the web contents DIP bounds.
  canvas->DrawImageInt(image, 0, 0, image.width(), image.height(), 0, 0,
                       wc_bounds_in_screen.width(),
                       wc_bounds_in_screen.height(), false, image_flags);
  canvas->Restore();

  // End blur layer
  canvas->Restore();

  // Apply Gradient Mask to fade out the blurred reflection
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

  // End gradient mask layer
  canvas->Restore();
}
