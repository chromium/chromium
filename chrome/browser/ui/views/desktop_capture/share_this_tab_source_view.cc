// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/desktop_capture/share_this_tab_source_view.h"

#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/ui/views/desktop_capture/rounded_corner_image_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "media/base/video_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Base UI measurements
constexpr int kPreviewWidth = 320;
constexpr int kPreviewHeight = 240;
constexpr int kPadding = 8;
constexpr int kFaviconWidth = 16;
constexpr int kFaviconTabTitleRowHeight = 20;

// Derived UI measurements
constexpr gfx::Rect kPreviewRect(kPadding,
                                 kPadding,
                                 kPreviewWidth,
                                 kPreviewHeight);
// TODO(crbug.com/40268977): Align favicon height properly with label.
constexpr gfx::Rect kFaviconRect(kPadding,
                                 kPreviewRect.bottom() + kPadding,
                                 kFaviconWidth,
                                 kFaviconTabTitleRowHeight);
constexpr gfx::Rect kTabTitleMaxRect(kFaviconRect.right() + kPadding,
                                     kPreviewRect.bottom() + kPadding,
                                     kPreviewWidth - kFaviconWidth - kPadding,
                                     kFaviconTabTitleRowHeight);

constexpr base::TimeDelta kUpdatePeriodMs = base::Milliseconds(250);

void HandleCapturedBitmap(
    base::OnceCallback<void(uint32_t, const std::optional<gfx::ImageSkia>&)>
        reply,
    std::optional<uint32_t> last_hash,
    gfx::Size thumbnail_size,
    const SkBitmap& bitmap) {
  CHECK(!thumbnail_size.IsEmpty());

  std::optional<gfx::ImageSkia> image;

  // Only scale and update if the frame appears to be new.
  const uint32_t hash = base::FastHash(base::make_span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize()));
  if (!last_hash.has_value() || hash != last_hash.value()) {
    image = ScaleBitmap(bitmap, thumbnail_size);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(reply), hash, image));
}

}  // namespace

ShareThisTabSourceView::ShareThisTabSourceView(
    base::WeakPtr<content::WebContents> web_contents)
    : web_contents_(web_contents),
      thumbnail_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(web_contents_);

  constexpr int kThrobberRadius = 14;
  constexpr gfx::Rect kThrobberRect(
      kPadding + kPreviewWidth / 2 - kThrobberRadius,
      kPadding + kPreviewHeight / 2 - kThrobberRadius, 2 * kThrobberRadius,
      2 * kThrobberRadius);
  throbber_ = AddChildView(std::make_unique<views::Throbber>());
  throbber_->SetBoundsRect(kThrobberRect);
  throbber_->Start();

  image_view_ = AddChildView(std::make_unique<RoundedCornerImageView>());
  image_view_->SetVisible(false);
  image_view_->SetBoundsRect(kPreviewRect);

  favicon_view_ = AddChildView(std::make_unique<views::ImageView>());
  favicon_view_->SetBoundsRect(kFaviconRect);

  tab_title_label_ = AddChildView(std::make_unique<views::Label>());
  tab_title_label_->SetBoundsRect(kTabTitleMaxRect);
  tab_title_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  tab_title_label_->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  tab_title_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  UpdateFaviconAndTabTitle();
}

ShareThisTabSourceView::~ShareThisTabSourceView() = default;

void ShareThisTabSourceView::Activate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  throbber_->Stop();
  throbber_->SetVisible(false);
  image_view_->SetVisible(true);
  refreshing_ = true;
  Refresh();
}

void ShareThisTabSourceView::StopRefreshing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  refreshing_ = false;
}

gfx::Size ShareThisTabSourceView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40262420): Use distances from LayoutProvider
  return gfx::Size(kPreviewWidth + 2 * kPadding,
                   kTabTitleMaxRect.bottom() + kPadding);
}

void ShareThisTabSourceView::UpdateFaviconAndTabTitle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents_) {
    return;
  }

  const gfx::Image favicon =
      favicon::TabFaviconFromWebContents(web_contents_.get());
  favicon_view_->SetImage(ui::ImageModel::FromImage(
      favicon.IsEmpty() ? favicon::GetDefaultFavicon() : favicon));

  tab_title_label_->SetText(web_contents_->GetTitle());
}

void ShareThisTabSourceView::Refresh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!refreshing_) {
    return;  // No further refreshes scheduled.
  }

  content::RenderFrameHost* const host = web_contents_->GetPrimaryMainFrame();
  if (!host) {
    return;
  }

  content::RenderWidgetHostView* const view = host->GetView();
  if (!view) {
    return;
  }

  auto reply = base::BindOnce(&ShareThisTabSourceView::OnCaptureHandled,
                              weak_factory_.GetWeakPtr());

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindPostTask(thumbnail_task_runner_,
                         base::BindOnce(&HandleCapturedBitmap, std::move(reply),
                                        last_hash_, kPreviewRect.size())));
}

void ShareThisTabSourceView::OnCaptureHandled(
    uint32_t hash,
    const std::optional<gfx::ImageSkia>& image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK((hash != last_hash_) == image.has_value());  // Only new frames passed.

  UpdateFaviconAndTabTitle();

  if (hash != last_hash_) {
    last_hash_ = hash;
    image_view_->SetImage(ui::ImageModel::FromImageSkia(image.value()));
  }

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShareThisTabSourceView::Refresh,
                     weak_factory_.GetWeakPtr()),
      kUpdatePeriodMs);
}

BEGIN_METADATA(ShareThisTabSourceView)
END_METADATA
