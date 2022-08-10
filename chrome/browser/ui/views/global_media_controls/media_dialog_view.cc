// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_footer_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_legacy_cast_footer_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_features.h"

using media_session::mojom::MediaSessionAction;

namespace {

static constexpr int kLiveCaptionBetweenChildSpacing = 4;
static constexpr int kLiveCaptionHorizontalMarginDip = 10;
static constexpr int kLiveCaptionImageWidthDip = 20;
static constexpr int kLiveCaptionVerticalMarginDip = 16;

std::u16string GetLiveCaptionTitle(PrefService* profile_prefs) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    return l10n_util::GetStringUTF16(
        IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_ENGLISH_ONLY);
  }
  // The selected language is only shown when Live Caption is enabled.
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
    int language_message_id = speech::GetLanguageDisplayName(
        prefs::GetLiveCaptionLanguageCode(profile_prefs));
    if (language_message_id) {
      std::u16string language = l10n_util::GetStringUTF16(language_message_id);
      return l10n_util::GetStringFUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_SHOW_LANGUAGE, language);
    }
  }
  return l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION);
}

}  // namespace

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
bool MediaDialogView::has_been_opened_ = false;

// static
views::Widget* MediaDialogView::ShowDialogFromToolbar(
    views::View* anchor_view,
    MediaNotificationService* service,
    Profile* profile) {
  return ShowDialog(
      anchor_view, views::BubbleBorder::TOP_RIGHT, service, profile, nullptr,
      global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon);
}

// static
views::Widget* MediaDialogView::ShowDialogCentered(
    const gfx::Rect& bounds,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  auto* widget = ShowDialog(nullptr, views::BubbleBorder::TOP_CENTER, service,
                            profile, contents, entry_point);
  instance_->SetAnchorRect(bounds);
  return widget;
}

// static
views::Widget* MediaDialogView::ShowDialog(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  DCHECK(service);
  // Hide the previous instance if it exists, since there can only be one dialog
  // instance at a time.
  HideDialog();
  instance_ = new MediaDialogView(anchor_view, anchor_position, service,
                                  profile, contents, entry_point);
  if (!anchor_view) {
    instance_->set_has_parent(false);
  }

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
    instance_->service_->media_item_manager()->SetDialogDelegate(nullptr);
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

global_media_controls::MediaItemUI* MediaDialogView::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  auto view = BuildMediaItemUIView(id, item);
  auto* view_ptr = view.get();
  view_ptr->AddObserver(this);
  observed_items_[id] = view_ptr;

  active_sessions_view_->ShowItem(id, std::move(view));
  UpdateBubbleSize();

  for (auto& observer : observers_)
    observer.OnMediaSessionShown();

  return view_ptr;
}

void MediaDialogView::HideMediaItem(const std::string& id) {
  active_sessions_view_->HideItem(id);

  if (active_sessions_view_->empty())
    HideDialog();
  else
    UpdateBubbleSize();

  for (auto& observer : observers_)
    observer.OnMediaSessionHidden();
}

void MediaDialogView::HideMediaDialog() {
  HideDialog();
}

void MediaDialogView::Focus() {
  RequestFocus();
}

void MediaDialogView::AddedToWidget() {
  int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  views::BubbleFrameView* frame = GetBubbleFrameView();
  if (frame) {
    frame->SetCornerRadius(corner_radius);
  }
  if (entry_point_ ==
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    service_->SetDialogDelegateForWebContents(
        this, web_contents_for_presentation_request_);
  } else {
    service_->media_item_manager()->SetDialogDelegate(this);
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
  if (!captions::IsLiveCaptionFeatureSupported())
    return;

  const int width = active_sessions_view_->GetPreferredSize().width();
  const int height = live_caption_container_->GetPreferredSize().height();
  live_caption_container_->SetPreferredSize(gfx::Size(width, height));
}

void MediaDialogView::OnMediaItemUISizeChanged() {
  UpdateBubbleSize();
}

void MediaDialogView::OnMediaItemUIMetadataChanged() {
  for (auto& observer : observers_)
    observer.OnMediaSessionMetadataUpdated();
}

void MediaDialogView::OnMediaItemUIActionsChanged() {
  for (auto& observer : observers_)
    observer.OnMediaSessionActionsChanged();
}

void MediaDialogView::OnMediaItemUIDestroyed(const std::string& id) {
  auto iter = observed_items_.find(id);
  DCHECK(iter != observed_items_.end());

  iter->second->RemoveObserver(this);
  observed_items_.erase(iter);
}

void MediaDialogView::AddObserver(MediaDialogViewObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaDialogView::RemoveObserver(MediaDialogViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::map<const std::string, global_media_controls::MediaItemUIView*>&
MediaDialogView::GetItemsForTesting() const {
  return active_sessions_view_->items_for_testing();  // IN-TEST
}

const global_media_controls::MediaItemUIListView*
MediaDialogView::GetListViewForTesting() const {
  return active_sessions_view_;
}

MediaDialogView::MediaDialogView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point)
    : BubbleDialogDelegateView(anchor_view, anchor_position),
      service_(service),
      profile_(profile->GetOriginalProfile()),
      active_sessions_view_(AddChildView(
          std::make_unique<global_media_controls::MediaItemUIListView>())),
      web_contents_for_presentation_request_(contents),
      entry_point_(entry_point) {
  // Enable layer based clipping to ensure children using layers are clipped
  // appropriately.
  SetPaintClientToLayer(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_DIALOG_NAME));
  DCHECK(service_);
}

MediaDialogView::~MediaDialogView() {
  for (auto item_pair : observed_items_)
    item_pair.second->RemoveObserver(this);
}

void MediaDialogView::Init() {
  // Remove margins.
  set_margins(gfx::Insets());
  if (!captions::IsLiveCaptionFeatureSupported()) {
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
              // TODO(crbug.com/1305767): The order of the parameters to
              // gfx::Insets::VH() seems wrong.
              gfx::Insets::VH(kLiveCaptionHorizontalMarginDip,
                              kLiveCaptionVerticalMarginDip),
              kLiveCaptionBetweenChildSpacing));

  auto live_caption_image = std::make_unique<views::ImageView>();
  live_caption_image->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kLiveCaptionOnIcon, ui::kColorIcon,
      kLiveCaptionImageWidthDip));
  live_caption_container->AddChildView(std::move(live_caption_image));

  std::u16string live_caption_title_message =
      GetLiveCaptionTitle(profile_->GetPrefs());
  auto live_caption_title =
      std::make_unique<views::Label>(live_caption_title_message);
  live_caption_title->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  live_caption_title->SetMultiLine(true);
  live_caption_title_ =
      live_caption_container->AddChildView(std::move(live_caption_title));
  live_caption_container_layout->SetFlexForView(live_caption_title_, 1);

  auto live_caption_button = std::make_unique<views::ToggleButton>(
      base::BindRepeating(&MediaDialogView::OnLiveCaptionButtonPressed,
                          base::Unretained(this)));
  live_caption_button->SetIsOn(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  live_caption_button->SetAccessibleName(live_caption_title_->GetText());
  live_caption_button_ =
      live_caption_container->AddChildView(std::move(live_caption_button));

  live_caption_container_ = AddChildView(std::move(live_caption_container));
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this) {
    instance_ = nullptr;
    service_->media_item_manager()->SetDialogDelegate(nullptr);
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

  // Do not update the title if SODA is currently downloading.
  if (!speech::SodaInstaller::GetInstance()->IsSodaDownloading(
          speech::GetLanguageCode(
              prefs::GetLiveCaptionLanguageCode(profile_->GetPrefs())))) {
    live_caption_title_->SetText(GetLiveCaptionTitle(profile_->GetPrefs()));
  }

  live_caption_button_->SetIsOn(enabled);
}

void MediaDialogView::OnSodaInstalled(speech::LanguageCode language_code) {
  if (!prefs::IsLanguageCodeForLiveCaption(language_code, profile_->GetPrefs()))
    return;
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  live_caption_title_->SetText(GetLiveCaptionTitle(profile_->GetPrefs()));
}

void MediaDialogView::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  // Check that language code matches the selected language for Live Caption or
  // is LanguageCode::kNone (signifying the SODA binary failed).
  if (!prefs::IsLanguageCodeForLiveCaption(language_code,
                                           profile_->GetPrefs()) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }

  live_caption_title_->SetText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_ERROR));
}

void MediaDialogView::OnSodaProgress(speech::LanguageCode language_code,
                                     int progress) {
  // Check that language code matches the selected language for Live Caption or
  // is LanguageCode::kNone (signifying the SODA binary has progress).
  if (!prefs::IsLanguageCodeForLiveCaption(language_code,
                                           profile_->GetPrefs()) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }
  live_caption_title_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_PROGRESS, progress));
}

std::unique_ptr<global_media_controls::MediaItemUIView>
MediaDialogView::BuildMediaItemUIView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  const bool is_cast_item =
      item->SourceType() == media_message_center::SourceType::kCast;
  const bool is_local_media_session =
      item->SourceType() ==
      media_message_center::SourceType::kLocalMediaSession;
  const bool gmc_cast_start_stop_enabled =
      media_router::GlobalMediaControlsCastStartStopEnabled(profile_);

  // Show a device selector view for media and supplemental notifications.
  std::unique_ptr<MediaItemUIDeviceSelectorView> device_selector_view;
  if (!is_cast_item && (gmc_cast_start_stop_enabled ||
                        base::FeatureList::IsEnabled(
                            media::kGlobalMediaControlsSeamlessTransfer))) {
    const bool show_expand_button =
        !base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI);
    std::unique_ptr<media_router::CastDialogController> cast_controller;
    if (gmc_cast_start_stop_enabled) {
      cast_controller =
          is_local_media_session
              ? service_->CreateCastDialogControllerForSession(id)
              : service_->CreateCastDialogControllerForPresentationRequest();
    }
    device_selector_view = std::make_unique<MediaItemUIDeviceSelectorView>(
        id, service_, std::move(cast_controller),
        /* has_audio_output */ is_local_media_session, entry_point_,
        show_expand_button);
  }

  base::RepeatingClosure stop_casting_closure =
      is_cast_item ? base::BindRepeating(
                         &CastMediaNotificationItem::StopCasting,
                         static_cast<CastMediaNotificationItem*>(item.get())
                             ->GetWeakPtr(),
                         entry_point_)
                   : base::NullCallback();

  std::unique_ptr<global_media_controls::MediaItemUIFooter> footer_view;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI)) {
    footer_view = std::make_unique<MediaItemUIFooterView>(stop_casting_closure);

    if (device_selector_view) {
      auto* modern_footer =
          static_cast<MediaItemUIFooterView*>(footer_view.get());
      modern_footer->SetDelegate(device_selector_view.get());
      device_selector_view->AddObserver(modern_footer);
    }
  } else if (is_cast_item && gmc_cast_start_stop_enabled) {
    footer_view =
        std::make_unique<MediaItemUILegacyCastFooterView>(stop_casting_closure);
  }

  return std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, std::move(footer_view), std::move(device_selector_view));
}

BEGIN_METADATA(MediaDialogView, views::BubbleDialogDelegateView)
END_METADATA
