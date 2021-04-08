// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"
#include "chrome/browser/ui/views/user_education/new_badge_label.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/views_features.h"

using media_session::mojom::MediaSessionAction;

namespace {

static constexpr int kLiveCaptionBetweenChildSpacing = 4;
static constexpr int kLiveCaptionHorizontalMarginDip = 10;
static constexpr int kLiveCaptionImageWidthDip = 20;
static constexpr int kLiveCaptionVerticalMarginDip = 16;

}  // namespace

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
bool MediaDialogView::has_been_opened_ = false;

// static
views::Widget* MediaDialogView::ShowDialog(
    views::View* anchor_view,
    MediaNotificationService* service,
    Profile* profile,
    GlobalMediaControlsEntryPoint entry_point) {
  return ShowDialogForPresentationRequest(anchor_view, service, profile,
                                          nullptr, entry_point);
}

// static
views::Widget* MediaDialogView::ShowDialogForPresentationRequest(
    views::View* anchor_view,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    GlobalMediaControlsEntryPoint entry_point) {
  DCHECK(!instance_);
  DCHECK(service);
  instance_ =
      new MediaDialogView(anchor_view, service, profile, contents, entry_point);

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();

  base::UmaHistogramBoolean("Media.GlobalMediaControls.RepeatUsage",
                            has_been_opened_);
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.EntryPoint",
                                entry_point);
  has_been_opened_ = true;

  return widget;
}

// static
void MediaDialogView::HideDialog() {
  if (IsShowing()) {
    instance_->service_->SetDialogDelegate(nullptr);
    speech::SodaInstaller::GetInstance()->RemoveObserver(instance_);
    instance_->GetWidget()->Close();
  }

  // Set |instance_| to nullptr so that |IsShowing()| returns false immediately.
  // We also set to nullptr in |WindowClosing()| (which happens asynchronously),
  // since |HideDialog()| is not always called.
  instance_ = nullptr;
}

// static
bool MediaDialogView::IsShowing() {
  return instance_ != nullptr;
}

MediaNotificationContainerImpl* MediaDialogView::ShowMediaSession(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  auto container = std::make_unique<MediaNotificationContainerImplView>(
      id, item, service_, entry_point_);
  MediaNotificationContainerImplView* container_ptr = container.get();
  container_ptr->AddObserver(this);
  observed_containers_[id] = container_ptr;

  active_sessions_view_->ShowNotification(id, std::move(container));
  UpdateBubbleSize();

  for (auto& observer : observers_)
    observer.OnMediaSessionShown();

  return container_ptr;
}

void MediaDialogView::HideMediaSession(const std::string& id) {
  active_sessions_view_->HideNotification(id);

  if (active_sessions_view_->empty())
    HideDialog();
  else
    UpdateBubbleSize();

  for (auto& observer : observers_)
    observer.OnMediaSessionHidden();
}

std::unique_ptr<OverlayMediaNotification> MediaDialogView::PopOut(
    const std::string& id,
    gfx::Rect bounds) {
  return active_sessions_view_->PopOut(id, bounds);
}

void MediaDialogView::HideMediaDialog() {
  HideDialog();
}

void MediaDialogView::AddedToWidget() {
  int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  views::BubbleFrameView* frame = GetBubbleFrameView();
  if (frame) {
    frame->SetCornerRadius(corner_radius);
  }
  if (entry_point_ == GlobalMediaControlsEntryPoint::kPresentation) {
    service_->SetDialogDelegateForWebContents(
        this, web_contents_for_presentation_request_);
  } else {
    service_->SetDialogDelegate(this);
  }
  speech::SodaInstaller::GetInstance()->AddObserver(this);
}

gfx::Size MediaDialogView::CalculatePreferredSize() const {
  // If we have active sessions, then fit to them.
  if (!active_sessions_view_->empty())
    return views::BubbleDialogDelegateView::CalculatePreferredSize();

  // Otherwise, use a standard size for bubble dialogs.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, 1);
}

void MediaDialogView::UpdateBubbleSize() {
  SizeToContents();
  if (!base::FeatureList::IsEnabled(media::kLiveCaption))
    return;

  const int width = active_sessions_view_->GetPreferredSize().width();
  const int height = live_caption_container_->GetPreferredSize().height();
  live_caption_container_->SetPreferredSize(gfx::Size(width, height));
}

void MediaDialogView::OnContainerSizeChanged() {
  UpdateBubbleSize();
}

void MediaDialogView::OnContainerMetadataChanged() {
  for (auto& observer : observers_)
    observer.OnMediaSessionMetadataUpdated();
}

void MediaDialogView::OnContainerActionsChanged() {
  for (auto& observer : observers_)
    observer.OnMediaSessionActionsChanged();
}

void MediaDialogView::OnContainerDestroyed(const std::string& id) {
  auto iter = observed_containers_.find(id);
  DCHECK(iter != observed_containers_.end());

  iter->second->RemoveObserver(this);
  observed_containers_.erase(iter);
}

void MediaDialogView::AddObserver(MediaDialogViewObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaDialogView::RemoveObserver(MediaDialogViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::map<const std::string, MediaNotificationContainerImplView*>&
MediaDialogView::GetNotificationsForTesting() const {
  return active_sessions_view_->notifications_for_testing();
}

const MediaNotificationListView* MediaDialogView::GetListViewForTesting()
    const {
  return active_sessions_view_;
}

MediaDialogView::MediaDialogView(views::View* anchor_view,
                                 MediaNotificationService* service,
                                 Profile* profile,
                                 content::WebContents* contents,
                                 GlobalMediaControlsEntryPoint entry_point)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      service_(service),
      profile_(profile->GetOriginalProfile()),
      active_sessions_view_(
          AddChildView(std::make_unique<MediaNotificationListView>())),
      web_contents_for_presentation_request_(contents),
      entry_point_(entry_point) {
  // Enable layer based clipping to ensure children using layers are clipped
  // appropriately.
  SetPaintClientToLayer(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  DCHECK(service_);
}

MediaDialogView::~MediaDialogView() {
  for (auto container_pair : observed_containers_)
    container_pair.second->RemoveObserver(this);
}

void MediaDialogView::Init() {
  // Remove margins.
  set_margins(gfx::Insets());
  if (!base::FeatureList::IsEnabled(media::kLiveCaption)) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    return;
  }
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  auto live_caption_container = std::make_unique<View>();
  auto* live_caption_container_layout =
      live_caption_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets(kLiveCaptionHorizontalMarginDip,
                          kLiveCaptionVerticalMarginDip),
              kLiveCaptionBetweenChildSpacing));

  auto live_caption_image = std::make_unique<views::ImageView>();
  live_caption_image->SetImage(gfx::CreateVectorIcon(
      vector_icons::kLiveCaptionOnIcon, kLiveCaptionImageWidthDip,
      SkColor(gfx::kGoogleGrey700)));
  live_caption_container->AddChildView(std::move(live_caption_image));

  // Live Caption multi language is only enabled when SODA is also enabled.
  const int live_caption_title_message =
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
              base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)
          ? IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION
          : IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_ENGLISH_ONLY;
  auto live_caption_title = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(live_caption_title_message));
  live_caption_title->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  live_caption_title->SetMultiLine(true);
  live_caption_title_ =
      live_caption_container->AddChildView(std::move(live_caption_title));
  live_caption_container_layout->SetFlexForView(live_caption_title_, 1);

  // Only create and show the new badge if Live Caption is not enabled at the
  // initialization of the MediaDialogView.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled)) {
    auto live_caption_title_new_badge = std::make_unique<NewBadgeLabel>(
        l10n_util::GetStringUTF16(live_caption_title_message));
    live_caption_title_new_badge->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    live_caption_title_new_badge_ = live_caption_container->AddChildView(
        std::move(live_caption_title_new_badge));
    live_caption_container_layout->SetFlexForView(live_caption_title_new_badge_,
                                                  1);
    live_caption_title_->SetVisible(false);
  }

  auto live_caption_button = std::make_unique<views::ToggleButton>(
      base::BindRepeating(&MediaDialogView::OnLiveCaptionButtonPressed,
                          base::Unretained(this)));
  live_caption_button->SetIsOn(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  live_caption_button->SetAccessibleName(live_caption_title_->GetText());
  live_caption_button->SetThumbOnColor(SkColor(gfx::kGoogleBlue600));
  live_caption_button->SetTrackOnColor(SkColorSetA(gfx::kGoogleBlue600, 128));
  live_caption_button->SetThumbOffColor(SK_ColorWHITE);
  live_caption_button->SetTrackOffColor(SkColor(gfx::kGoogleGrey400));
  live_caption_button_ =
      live_caption_container->AddChildView(std::move(live_caption_button));

  live_caption_container_ = AddChildView(std::move(live_caption_container));
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this) {
    instance_ = nullptr;
    service_->SetDialogDelegate(nullptr);
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }
}

void MediaDialogView::OnLiveCaptionButtonPressed() {
  bool enabled = !profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled);
  ToggleLiveCaption(enabled);
  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.EnableFromGlobalMediaControls", enabled);
}

void MediaDialogView::ToggleLiveCaption(bool enabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, enabled);
  live_caption_button_->SetIsOn(enabled);
  if (live_caption_title_new_badge_ &&
      live_caption_title_new_badge_->GetVisible()) {
    live_caption_title_->SetVisible(true);
    live_caption_title_new_badge_->SetVisible(false);
  }
}

void MediaDialogView::OnSodaInstalled() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  // Live Caption multi language is only enabled when SODA is also enabled.
  const int live_caption_title_message =
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
              base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)
          ? IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION
          : IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_ENGLISH_ONLY;
  live_caption_title_->SetText(
      l10n_util::GetStringUTF16(live_caption_title_message));
}

void MediaDialogView::OnSodaError() {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    ToggleLiveCaption(false);
  }

  live_caption_title_->SetText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_ERROR));
}

void MediaDialogView::OnSodaProgress(int progress) {
  live_caption_title_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_PROGRESS, progress));
}

BEGIN_METADATA(MediaDialogView, views::BubbleDialogDelegateView)
END_METADATA
