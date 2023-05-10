// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/share_this_tab_source_view.h"

#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "media/base/video_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr gfx::Size kPreviewSize(320, 240);
constexpr int kPadding = 8;
constexpr base::TimeDelta kUpdatePeriodMs = base::Milliseconds(250);

void HandleCapturedBitmap(
    base::OnceCallback<void(uint32_t, const absl::optional<gfx::ImageSkia>&)>
        reply,
    absl::optional<uint32_t> last_hash,
    gfx::Size thumbnail_size,
    const SkBitmap& bitmap) {
  CHECK(!thumbnail_size.IsEmpty());

  absl::optional<gfx::ImageSkia> image;

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
  View* throbber_container = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* throbber_layout =
      throbber_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  throbber_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  throbber_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  // TODO(crbug.com/1428878): Use distances from LayoutProvider
  throbber_container->SetBoundsRect(
      gfx::Rect(gfx::Point(kPadding, kPadding), kPreviewSize));
  throbber_container->SetCanProcessEventsWithinSubtree(false);
  throbber_ =
      throbber_container->AddChildView(std::make_unique<views::Throbber>());
  throbber_->Start();

  image_view_ = AddChildView(std::make_unique<views::ImageView>());
  image_view_->SetVisible(false);
  image_view_->SetBoundsRect(gfx::Rect(8, 8, 320, 240));
}

ShareThisTabSourceView::~ShareThisTabSourceView() = default;

void ShareThisTabSourceView::Activate() {
  throbber_->Stop();
  throbber_->SetVisible(false);
  image_view_->SetVisible(true);
  refreshing_ = true;
  Refresh();
}

void ShareThisTabSourceView::StopRefreshing() {
  refreshing_ = false;
}

gfx::Size ShareThisTabSourceView::CalculatePreferredSize() const {
  // TODO(crbug.com/1428878): Use distances from LayoutProvider
  return kPreviewSize + gfx::Size(2 * kPadding, 2 * kPadding);
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
                                        last_hash_, gfx::Size(320, 240))));
}

void ShareThisTabSourceView::OnCaptureHandled(
    uint32_t hash,
    const absl::optional<gfx::ImageSkia>& image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK((hash != last_hash_) == image.has_value());  // Only new frames passed.

  if (hash != last_hash_) {
    last_hash_ = hash;
    image_view_->SetImage(image.value());
  }

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShareThisTabSourceView::Refresh,
                     weak_factory_.GetWeakPtr()),
      kUpdatePeriodMs);
}
