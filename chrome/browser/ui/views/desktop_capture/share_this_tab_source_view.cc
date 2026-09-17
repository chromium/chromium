// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/share_this_tab_source_view.h"

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/favicon/content/content_favicon_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "media/base/video_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/layout/layout_provider.h"

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
    const content::CopyFromSurfaceResult& result) {
  CHECK(!thumbnail_size.IsEmpty());

  // TODO(crbug.com/466199824): Update callsite to handle error case.
  const SkBitmap& bitmap = result.has_value() ? result->bitmap : SkBitmap();
  std::optional<gfx::ImageSkia> image;

  // Only scale and update if the frame appears to be new.
  const uint32_t hash = base::FastHash(UNSAFE_TODO(base::span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize())));
  if (!last_hash.has_value() || hash != last_hash.value()) {
    image = ScaleBitmap(bitmap, thumbnail_size);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(reply), hash, image));
}

}  // namespace

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
// static
bool ShareThisTabSourceView::IsTabSharingBlocked(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          enterprise_data_protection::kEnableTabSharingProtection) ||
      !web_contents) {
    return false;
  }
  if (enterprise_data_protection::IsScreenShareBlocked(web_contents)) {
    return true;
  }
  auto* page_user_data =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(
          web_contents->GetPrimaryPage());
  return page_user_data && !page_user_data->settings().allow_screenshots;
}
#endif

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

  image_view_ = AddChildView(std::make_unique<views::ImageView>());
  image_view_->SetCanProcessEventsWithinSubtree(false);
  image_view_->SetCornerRadius(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium));
  image_view_->SetVisible(false);
  image_view_->SetBoundsRect(kPreviewRect);

  blocked_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_BLOCKED_PREVIEW)));
  blocked_label_->SetBoundsRect(kPreviewRect);
  blocked_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  blocked_label_->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  blocked_label_->SetEnabledColor(ui::kColorSysOnTonalContainer);
  blocked_label_->SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysTonalContainer,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium)));
  blocked_label_->SetBackgroundColor(ui::kColorSysTonalContainer);
  blocked_label_->SetVisible(false);

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
  activated_ = true;
  UpdateBlockedState();
}

void ShareThisTabSourceView::StopRefreshing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  refreshing_ = false;
}

void ShareThisTabSourceView::UpdateBlockedState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateFaviconAndTabTitle();

  if (!activated_) {
    return;
  }

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  if (IsTabSharingBlocked(web_contents_.get())) {
    StopRefreshing();
    image_view_->SetVisible(false);
    blocked_label_->SetVisible(true);
    return;
  }
#endif

  blocked_label_->SetVisible(false);
  image_view_->SetVisible(true);
  if (!refreshing_) {
    refreshing_ = true;
    Refresh();
  }
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

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  if (IsTabSharingBlocked(web_contents_.get())) {
    favicon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? vector_icons::kDomainIcon
                                          : vector_icons::kBusinessOldIcon,
        ui::kColorIcon, kFaviconWidth));
    favicon_view_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_BLOCKED_TITLE));
    tab_title_label_->SetText(web_contents_->GetTitle());
    return;
  }
#endif

  favicon_view_->SetTooltipText(std::u16string());
  const gfx::Image favicon =
      favicon::GetTabFaviconMaybeDesaturatedOnError(web_contents_.get());
  favicon_view_->SetImage(ui::ImageModel::FromImage(
      favicon.IsEmpty() ? favicon::GetDefaultFavicon() : favicon));

  tab_title_label_->SetText(web_contents_->GetTitle());
}

void ShareThisTabSourceView::Refresh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!refreshing_) {
    return;  // No further refreshes scheduled.
  }

  if (!web_contents_) {
    return;
  }

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  if (IsTabSharingBlocked(web_contents_.get())) {
    return;
  }
#endif

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
      gfx::Rect(), gfx::Size(), base::TimeDelta(),
      base::BindPostTask(thumbnail_task_runner_,
                         base::BindOnce(&HandleCapturedBitmap, std::move(reply),
                                        last_hash_, kPreviewRect.size())));
}

void ShareThisTabSourceView::OnCaptureHandled(
    uint32_t hash,
    const std::optional<gfx::ImageSkia>& image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK((hash != last_hash_) == image.has_value());  // Only new frames passed.

  if (!refreshing_) {
    return;
  }

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  if (IsTabSharingBlocked(web_contents_.get())) {
    return;
  }
#endif

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
