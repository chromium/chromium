// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"

#include "base/metrics/histogram_functions.h"
#include "components/global_media_controls/media_view_utils.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace global_media_controls {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr int kFixedWidth = 400;

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::VH(16, 12);
constexpr gfx::Insets kArtworkRowInsets = gfx::Insets::TLBR(0, 4, 0, 0);
constexpr gfx::Insets kMetadataRowInsets = gfx::Insets::TLBR(0, 0, 0, 4);

constexpr int kBackgroundCornerRadius = 8;
constexpr int kArtworkCornerRadius = 8;
constexpr int kFaviconCornerRadius = 2;

constexpr int kBackgroundSeparator = 12;
constexpr int kArtworkRowSeparator = 16;
constexpr int kInfoColumnSeparator = 4;
constexpr int kFaviconSourceSeparator = 4;
constexpr int kMetadataRowSeparator = 16;
constexpr int kMetadataColumnSeparator = 4;

constexpr int kPlayPauseButtonIconSize = 24;
constexpr int kMediaActionButtonIconSize = 20;

constexpr float kFocusRingHaloInset = -3.0f;

constexpr gfx::Size kArtworkSize = gfx::Size(80, 80);
constexpr gfx::Size kFaviconSize = gfx::Size(14, 14);
constexpr gfx::Size kPlayPauseButtonSize = gfx::Size(48, 48);
constexpr gfx::Size kMediaActionButtonSize = gfx::Size(28, 28);

// Buttons with the following media actions are in the progress row, on the two
// sides of the progress view.
const MediaSessionAction kProgressRowMediaActions[] = {
    MediaSessionAction::kPreviousTrack, MediaSessionAction::kNextTrack,
    MediaSessionAction::kSeekForward, MediaSessionAction::kSeekBackward};

// Media actions for the replay and forward 10 seconds buttons.
const MediaSessionAction kProgressRowSeekMediaActions[] = {
    MediaSessionAction::kSeekForward, MediaSessionAction::kSeekBackward};

}  // namespace

MediaItemUIUpdatedView::MediaItemUIUpdatedView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    media_message_center::MediaColorTheme media_color_theme,
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
    std::unique_ptr<MediaItemUIFooter> footer_view)
    : id_(id), item_(std::move(item)), media_color_theme_(media_color_theme) {
  CHECK(item_);

  SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme_.background_color_id, kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets,
      kBackgroundSeparator));

  views::FocusRing::Install(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kBackgroundCornerRadius);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetColorId(media_color_theme_.focus_ring_color_id);

  // |artwork_row| holds everything above the |progress_row|, starting with the
  // media artwork along with some media information and media buttons.
  auto* artwork_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  artwork_row->SetInsideBorderInsets(kArtworkRowInsets);
  artwork_row->SetBetweenChildSpacing(kArtworkRowSeparator);

  artwork_view_ =
      artwork_row->AddChildView(std::make_unique<views::ImageView>());
  artwork_view_->SetPreferredSize(kArtworkSize);
  artwork_view_->SetClipPath(
      SkPath().addRoundRect(RectToSkRect(gfx::Rect(kArtworkSize)),
                            kArtworkCornerRadius, kArtworkCornerRadius));
  artwork_view_->SetVisible(false);

  // |info_column| inside |artwork_row| right to the |artwork_view| holds the
  // |source_row| and |metadata_row|.
  auto* info_column =
      artwork_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  info_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  info_column->SetBetweenChildSpacing(kInfoColumnSeparator);
  artwork_row->SetFlexForView(info_column, 1);

  // |source_row| inside |info_column| holds the media favicon view, media
  // source label, start casting button and picture-in-picture button.
  auto* source_row =
      info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // |favicon_source| inside |source_row| holds the media favicon view and the
  // media source label to add a gap between them.
  auto* favicon_source =
      source_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  favicon_source->SetBetweenChildSpacing(kFaviconSourceSeparator);
  source_row->SetFlexForView(favicon_source, 1);

  // Create the media favicon view and initialize it with the default icon.
  favicon_view_ =
      favicon_source->AddChildView(std::make_unique<views::ImageView>());
  favicon_view_->SetPreferredSize(kFaviconSize);
  favicon_view_->SetClipPath(
      SkPath().addRoundRect(RectToSkRect(gfx::Rect(kFaviconSize)),
                            kFaviconCornerRadius, kFaviconCornerRadius));
  UpdateWithFavicon(gfx::ImageSkia());

  // Create the media source label.
  source_label_ = favicon_source->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_5));
  source_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  source_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  source_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  source_label_->SetEnabledColorId(
      media_color_theme_.secondary_foreground_color_id);
  favicon_source->SetFlexForView(source_label_, 1);

  // Create the start casting button.
  start_casting_button_ = CreateMediaActionButton(
      source_row, kEmptyMediaActionButtonId, vector_icons::kCastIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
  start_casting_button_->SetVisible(false);

  // Create the picture-in-picture button.
  picture_in_picture_button_ = CreateMediaActionButton(
      source_row, static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
      vector_icons::kPictureInPictureAltIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);

  // Create the casting indicator view which is visible when footer view is
  // shown.
  casting_indicator_view_ =
      source_row->AddChildView(std::make_unique<views::ImageView>());
  casting_indicator_view_->SetPreferredSize(kMediaActionButtonSize);
  casting_indicator_view_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kCastIcon,
      media_color_theme_.device_selector_foreground_color_id,
      kMediaActionButtonIconSize));

  // |metadata_row| inside |info_column| holds the |metadata_column| and
  // |play_pause_button_container|.
  auto* metadata_row =
      info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  metadata_row->SetInsideBorderInsets(kMetadataRowInsets);
  metadata_row->SetBetweenChildSpacing(kMetadataRowSeparator);
  metadata_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);

  auto* metadata_column =
      metadata_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  metadata_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  metadata_column->SetBetweenChildSpacing(kMetadataColumnSeparator);
  metadata_row->SetFlexForView(metadata_column, 1);

  // |metadata_column| inside |metadata_row| holds the media title label and
  // media artist label.
  title_label_ = metadata_column->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_2_BOLD));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  title_label_->SetEnabledColorId(
      media_color_theme_.primary_foreground_color_id);

  artist_label_ = metadata_column->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_2));
  artist_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  artist_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  artist_label_->SetEnabledColorId(
      media_color_theme_.primary_foreground_color_id);

  // |play_pause_button_container| inside |metadata_row| holds the play pause
  // button.
  auto* play_pause_button_container =
      metadata_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  play_pause_button_ = CreateMediaActionButton(
      play_pause_button_container, static_cast<int>(MediaSessionAction::kPlay),
      vector_icons::kPlayArrowIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY);
  play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme_.play_button_container_color_id,
      kPlayPauseButtonSize.height() / 2));

  // |progress_row| holds some media action buttons, the progress view and the
  // progress timestamp views.
  auto* progress_row = AddChildView(std::make_unique<views::BoxLayoutView>());

  // Create the current timestamp label before the progress view.
  current_timestamp_label_ =
      progress_row->AddChildView(std::make_unique<views::Label>(
          std::u16string(), views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_5));
  current_timestamp_label_->SetEnabledColorId(
      media_color_theme_.secondary_foreground_color_id);

  // Create the previous track button.
  CreateMediaActionButton(
      progress_row, static_cast<int>(MediaSessionAction::kPreviousTrack),
      vector_icons::kSkipPreviousIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK);

  // Create the replay 10 button.
  CreateMediaActionButton(
      progress_row, static_cast<int>(MediaSessionAction::kSeekBackward),
      vector_icons::kReplay10Icon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_REPLAY_10);

  // Create the progress view.
  progress_view_ =
      progress_row->AddChildView(std::make_unique<MediaProgressView>(
          /*use_squiggly_line=*/false,
          media_color_theme_.playing_progress_foreground_color_id,
          media_color_theme_.playing_progress_background_color_id,
          media_color_theme_.paused_progress_foreground_color_id,
          media_color_theme_.paused_progress_background_color_id,
          media_color_theme_.focus_ring_color_id,
          base::BindRepeating(
              &MediaItemUIUpdatedView::OnProgressDragStateChange,
              base::Unretained(this)),
          base::BindRepeating(
              &MediaItemUIUpdatedView::OnPlaybackStateChangeForProgressDrag,
              base::Unretained(this)),
          base::BindRepeating(&MediaItemUIUpdatedView::SeekTo,
                              base::Unretained(this)),
          base::BindRepeating(
              &MediaItemUIUpdatedView::OnProgressViewUpdateProgress,
              base::Unretained(this))));
  progress_row->SetFlexForView(progress_view_, 1);

  // Create the live status view besides the progress view while only either
  // |progress_view_| or |live_status_view_| will show.
  live_status_view_ =
      progress_row->AddChildView(std::make_unique<MediaLiveStatusView>(
          media_color_theme_.play_button_foreground_color_id,
          media_color_theme_.play_button_container_color_id));
  progress_row->SetFlexForView(live_status_view_, 1);
  live_status_view_->SetVisible(false);

  // Create the forward 10 button.
  CreateMediaActionButton(
      progress_row, static_cast<int>(MediaSessionAction::kSeekForward),
      vector_icons::kForward10Icon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_FORWARD_10);

  // Create the next track button.
  CreateMediaActionButton(
      progress_row, static_cast<int>(MediaSessionAction::kNextTrack),
      vector_icons::kSkipNextIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK);

  // Create the duration timestamp label after the progress view.
  duration_timestamp_label_ =
      progress_row->AddChildView(std::make_unique<views::Label>(
          std::u16string(), views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_5));
  duration_timestamp_label_->SetEnabledColorId(
      media_color_theme_.secondary_foreground_color_id);

  // Add the device selector view below the |progress_row| if there is one.
  UpdateDeviceSelectorView(std::move(device_selector_view));

  // Add the cast device footer view below the |progress_row| if there is one.
  // It will only show up when this media item is being casted to another
  // device.
  UpdateFooterView(std::move(footer_view));

  // Set the timestamp labels to use fixed width so that they can replace the
  // media action buttons without changing the progress view's position.
  int timestamp_label_width = kMediaActionButtonSize.width() * 2;
  current_timestamp_label_->SetMultiLine(true);
  current_timestamp_label_->SizeToFit(timestamp_label_width);
  duration_timestamp_label_->SetMultiLine(true);
  duration_timestamp_label_->SizeToFit(timestamp_label_width);

  // Set the timestamp labels to be hidden initially.
  UpdateTimestampLabelsVisibility();

  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  UpdateAccessibleName();
  title_label_changed_callback_ = title_label_->AddTextChangedCallback(
      base::BindRepeating(&MediaItemUIUpdatedView::UpdateAccessibleName,
                          base::Unretained(this)));

  // Always register for media callbacks in the end after all the components are
  // initialized.
  item_->SetView(this);
}

MediaItemUIUpdatedView::~MediaItemUIUpdatedView() {
  if (item_) {
    item_->SetView(nullptr);
  }
  for (auto& observer : observers_) {
    observer.OnMediaItemUIDestroyed(id_);
  }
  observers_.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

gfx::Size MediaItemUIUpdatedView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto size = GetLayoutManager()->GetPreferredSize(this);
  return gfx::Size(kFixedWidth, size.height());
}

void MediaItemUIUpdatedView::AddedToWidget() {
  // Ink drop on the start casting button requires color provider to be ready,
  // so we need to update the state after the widget is ready.
  if (device_selector_view_) {
    UpdateCastingState();
  }
}

void MediaItemUIUpdatedView::UpdateAccessibleName() {
  if (title_label_->GetText().empty()) {
    GetViewAccessibility().SetName(l10n_util::GetStringUTF8(
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
  } else {
    GetViewAccessibility().SetName(title_label_->GetText());
  }
}

bool MediaItemUIUpdatedView::OnKeyPressed(const ui::KeyEvent& event) {
  // As soon as the media view gets the focus, it should be able to handle key
  // events that can change the progress.
  return progress_view_->OnKeyPressed(event);
}

bool MediaItemUIUpdatedView::OnMousePressed(const ui::MouseEvent& event) {
  // Activate the original source page if it exists when any part of the media
  // background view is pressed.
  for (auto& observer : observers_) {
    observer.OnMediaItemUIClicked(id_, /*activate_original_media=*/true);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// MediaItemUI implementations:

void MediaItemUIUpdatedView::AddObserver(MediaItemUIObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaItemUIUpdatedView::RemoveObserver(MediaItemUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

///////////////////////////////////////////////////////////////////////////////
// media_message_center::MediaNotificationView implementations:

void MediaItemUIUpdatedView::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  if (playing) {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPause), vector_icons::kPauseIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE,
        media_color_theme_.pause_button_foreground_color_id);
    play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        media_color_theme_.pause_button_container_color_id,
        kPlayPauseButtonSize.height() / 2));
  } else {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPlay),
        vector_icons::kPlayArrowIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY,
        media_color_theme_.play_button_foreground_color_id);
    play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        media_color_theme_.play_button_container_color_id,
        kPlayPauseButtonSize.height() / 2));
  }

  in_picture_in_picture_ =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  if (in_picture_in_picture_) {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kExitPictureInPicture),
        vector_icons::kPipExitIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP,
        media_color_theme_.secondary_foreground_color_id);
  } else {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
        vector_icons::kPictureInPictureAltIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP,
        media_color_theme_.secondary_foreground_color_id);
  }

  UpdateMediaActionButtonsVisibility();
}

void MediaItemUIUpdatedView::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  source_label_->SetText(metadata.source_title);
  title_label_->SetText(metadata.title);
  artist_label_->SetText(metadata.artist);
  for (auto& observer : observers_) {
    observer.OnMediaItemUIMetadataChanged();
  }
}

void MediaItemUIUpdatedView::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  media_actions_ = actions;
  UpdateMediaActionButtonsVisibility();
  for (auto& observer : observers_) {
    observer.OnMediaItemUIActionsChanged();
  }
}

void MediaItemUIUpdatedView::UpdateWithMediaPosition(
    const media_session::MediaPosition& position) {
  position_ = position;
  progress_view_->UpdateProgress(position);
  duration_timestamp_label_->SetText(GetFormattedDuration(position.duration()));

  // Show either the live status view or the progress view based on whether the
  // media is live. The media action buttons in the progress row may also change
  // their visibility.
  is_live_ = position.duration().is_max();
  live_status_view_->SetVisible(is_live_);
  progress_view_->SetVisible(!is_live_);
  UpdateMediaActionButtonsVisibility();
}

void MediaItemUIUpdatedView::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Hide the image so the other contents will adjust to fill the container.
    artwork_view_->SetVisible(false);
  } else {
    artwork_view_->SetVisible(true);
    artwork_view_->SetImageSize(
        ScaleImageSizeToFitView(image.size(), kArtworkSize));
    artwork_view_->SetImage(ui::ImageModel::FromImageSkia(image));
  }
}

void MediaItemUIUpdatedView::UpdateWithFavicon(const gfx::ImageSkia& icon) {
  if (icon.isNull()) {
    favicon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kGlobeIcon,
        media_color_theme_.primary_foreground_color_id, kFaviconSize.width()));
  } else {
    favicon_view_->SetImageSize(
        ScaleImageSizeToFitView(icon.size(), kFaviconSize));
    favicon_view_->SetImage(ui::ImageModel::FromImageSkia(icon));
  }
}

void MediaItemUIUpdatedView::UpdateDeviceSelectorVisibility(bool visible) {
  // The device selector view can change its device list visibility and we need
  // to update the casting state for it too.
  UpdateCastingState();
}

void MediaItemUIUpdatedView::UpdateDeviceSelectorAvailability(
    bool has_devices) {
  // Do not show the start casting button for a casting media item. Only show it
  // if there are available devices in the selector view.
  bool visible = has_devices && !footer_view_;
  if (visible != start_casting_button_->GetVisible()) {
    start_casting_button_->SetVisible(visible);
  }
}

///////////////////////////////////////////////////////////////////////////////
// MediaItemUIUpdatedView implementations:

void MediaItemUIUpdatedView::UpdateDeviceSelectorView(
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view) {
  // Remove the existing device selector view.
  if (device_selector_view_) {
    RemoveChildViewT(device_selector_view_.ExtractAsDangling());
    start_casting_button_->SetCallback(views::Button::PressedCallback());
  }
  // Add the new device selector view.
  if (device_selector_view) {
    device_selector_view_ = AddChildView(std::move(device_selector_view));
    device_selector_view_->SetMediaItemUIUpdatedView(this);
    start_casting_button_->SetCallback(
        base::BindRepeating(&MediaItemUIUpdatedView::StartCastingButtonPressed,
                            base::Unretained(this)));
  }
}

void MediaItemUIUpdatedView::UpdateFooterView(
    std::unique_ptr<MediaItemUIFooter> footer_view) {
  // Remove the existing footer view.
  if (footer_view_) {
    RemoveChildViewT(footer_view_.ExtractAsDangling());
  }
  // Add the new footer view.
  if (footer_view) {
    footer_view_ = AddChildView(std::move(footer_view));
  }
  // Casting indicator view should only show when the footer view is shown.
  casting_indicator_view_->SetVisible(footer_view_);
  // Footer view changes can change the picture-in-picture button's visibility.
  UpdateMediaActionButtonsVisibility();
}

void MediaItemUIUpdatedView::UpdateDeviceSelectorIssue(bool has_issue) {
  start_casting_button_->UpdateIcon(has_issue ? vector_icons::kCastWarningIcon
                                              : vector_icons::kCastIcon);
}

MediaActionButton* MediaItemUIUpdatedView::CreateMediaActionButton(
    views::View* parent,
    int button_id,
    const gfx::VectorIcon& vector_icon,
    int tooltip_text_id) {
  auto button = std::make_unique<MediaActionButton>(
      views::Button::PressedCallback(), button_id, tooltip_text_id,
      (button_id == static_cast<int>(MediaSessionAction::kPlay)
           ? kPlayPauseButtonIconSize
           : kMediaActionButtonIconSize),
      vector_icon,
      (button_id == static_cast<int>(MediaSessionAction::kPlay)
           ? kPlayPauseButtonSize
           : kMediaActionButtonSize),
      media_color_theme_.secondary_foreground_color_id,
      media_color_theme_.paused_progress_foreground_color_id,
      media_color_theme_.focus_ring_color_id);
  auto* button_ptr = parent->AddChildView(std::move(button));

  if (button_id != kEmptyMediaActionButtonId) {
    button_ptr->SetCallback(
        base::BindRepeating(&MediaItemUIUpdatedView::MediaActionButtonPressed,
                            base::Unretained(this), button_ptr));
    media_action_buttons_.push_back(button_ptr);
  }
  return button_ptr;
}

void MediaItemUIUpdatedView::MediaActionButtonPressed(views::Button* button) {
  switch (static_cast<MediaSessionAction>(button->GetID())) {
    case MediaSessionAction::kPlay:
    case MediaSessionAction::kPause:
    case MediaSessionAction::kPreviousTrack:
    case MediaSessionAction::kNextTrack:
    case MediaSessionAction::kSeekBackward:
    case MediaSessionAction::kSeekForward:
      base::UmaHistogramEnumeration(
          kMediaItemUIUpdatedViewActionHistogram,
          static_cast<MediaItemUIUpdatedViewAction>(button->GetID()));
      break;
    case MediaSessionAction::kEnterPictureInPicture:
      base::UmaHistogramEnumeration(
          kMediaItemUIUpdatedViewActionHistogram,
          MediaItemUIUpdatedViewAction::kEnterPictureInPicture);
      break;
    case MediaSessionAction::kExitPictureInPicture:
      base::UmaHistogramEnumeration(
          kMediaItemUIUpdatedViewActionHistogram,
          MediaItemUIUpdatedViewAction::kExitPictureInPicture);
      break;
    default:
      NOTREACHED();
  }

  // Make the screen reader announce the button text for accessibility since
  // there are only visual changes outside these buttons when they are clicked.
  if (base::Contains(kProgressRowMediaActions,
                     static_cast<MediaSessionAction>(button->GetID()))) {
    GetViewAccessibility().AnnouncePolitely(button->GetTooltipText());
  }

  if (button->GetID() == static_cast<int>(MediaSessionAction::kSeekBackward)) {
    item_->SeekTo(
        std::max(base::Seconds(0), position_.GetPosition() - kSeekTime));
    return;
  }
  if (button->GetID() == static_cast<int>(MediaSessionAction::kSeekForward)) {
    item_->SeekTo(
        std::min(position_.GetPosition() + kSeekTime, position_.duration()));
    return;
  }
  item_->OnMediaSessionActionButtonPressed(
      static_cast<MediaSessionAction>(button->GetID()));
}

void MediaItemUIUpdatedView::UpdateMediaActionButtonsVisibility() {
  bool should_invalidate_layout = false;

  for (views::Button* button : media_action_buttons_) {
    bool should_show = base::Contains(
        media_actions_, static_cast<MediaSessionAction>(button->GetID()));
    // Do not show the picture-in-picture button for a casting media item.
    if (button == picture_in_picture_button_ && footer_view_) {
      should_show = false;
    }

    if (base::Contains(kProgressRowMediaActions,
                       static_cast<MediaSessionAction>(button->GetID()))) {
      if (is_live_) {
        // For live media, the seek progress buttons are always hidden and the
        // other buttons are visible if their media actions are supported.
        if (base::Contains(kProgressRowSeekMediaActions,
                           static_cast<MediaSessionAction>(button->GetID()))) {
          should_show = false;
        }
      } else {
        // For non-live media, the progress row buttons should always be visible
        // when the user is not dragging the progress view. They should be
        // disabled if their media actions are not supported.
        button->SetEnabled(should_show);
        should_show = (drag_state_ == DragState::kDragEnded);
      }
    }

    if (should_show != button->GetVisible()) {
      button->SetVisible(should_show);
      should_invalidate_layout = true;
    }
  }

  if (should_invalidate_layout) {
    InvalidateLayout();
  }
}

void MediaItemUIUpdatedView::UpdateTimestampLabelsVisibility() {
  current_timestamp_label_->SetVisible(drag_state_ == DragState::kDragStarted);
  duration_timestamp_label_->SetVisible(drag_state_ == DragState::kDragStarted);
}

void MediaItemUIUpdatedView::OnProgressDragStateChange(DragState drag_state) {
  drag_state_ = drag_state;
  UpdateMediaActionButtonsVisibility();
  UpdateTimestampLabelsVisibility();

  // Record to metrics whether user is using the progress view to seek forward
  // or backward.
  if (drag_state_ == DragState::kDragStarted) {
    position_on_drag_started_ = position_.GetPosition();
  } else if (!position_on_drag_started_.is_max()) {
    if (position_.GetPosition() > position_on_drag_started_) {
      base::UmaHistogramEnumeration(
          kMediaItemUIUpdatedViewActionHistogram,
          MediaItemUIUpdatedViewAction::kProgressViewSeekForward);
    } else {
      base::UmaHistogramEnumeration(
          kMediaItemUIUpdatedViewActionHistogram,
          MediaItemUIUpdatedViewAction::kProgressViewSeekBackward);
    }
    position_on_drag_started_ = base::TimeDelta::Max();
  }
}

void MediaItemUIUpdatedView::OnPlaybackStateChangeForProgressDrag(
    PlaybackStateChangeForDragging change) {
  const auto action =
      (change == PlaybackStateChangeForDragging::kPauseForDraggingStarted
           ? MediaSessionAction::kPause
           : MediaSessionAction::kPlay);
  item_->OnMediaSessionActionButtonPressed(action);
}

void MediaItemUIUpdatedView::SeekTo(double seek_progress) {
  item_->SeekTo(seek_progress * position_.duration());
}

void MediaItemUIUpdatedView::OnProgressViewUpdateProgress(
    base::TimeDelta current_timestamp) {
  current_timestamp_label_->SetText(GetFormattedDuration(current_timestamp));
}

void MediaItemUIUpdatedView::StartCastingButtonPressed() {
  CHECK(device_selector_view_);
  if (device_selector_view_->IsDeviceSelectorExpanded()) {
    device_selector_view_->HideDevices();
    base::UmaHistogramEnumeration(
        kMediaItemUIUpdatedViewActionHistogram,
        MediaItemUIUpdatedViewAction::kHideDeviceListForCasting);
  } else {
    device_selector_view_->ShowDevices();
    base::UmaHistogramEnumeration(
        kMediaItemUIUpdatedViewActionHistogram,
        MediaItemUIUpdatedViewAction::kShowDeviceListForCasting);
  }
  start_casting_button_->GetViewAccessibility().AnnouncePolitely(
      start_casting_button_->GetTooltipText());
}

void MediaItemUIUpdatedView::UpdateCastingState() {
  CHECK(device_selector_view_);

  if (start_casting_button_->GetVisible()) {
    bool is_expanded = device_selector_view_->IsDeviceSelectorExpanded();
    if (is_expanded) {
      // Use the ink drop color as the button background if user clicks the
      // button to show devices.
      views::InkDrop::Get(start_casting_button_)
          ->GetInkDrop()
          ->SnapToActivated();

      // Indicate the user can hide the device list.
      start_casting_button_->UpdateText(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_HIDE_DEVICE_LIST);
    } else {
      // Hide the ink drop color if user clicks the button to hide devices.
      views::InkDrop::Get(start_casting_button_)->GetInkDrop()->SnapToHidden();

      // Indicate the user can show the device list.
      start_casting_button_->UpdateText(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
    }
  }

  for (auto& observer : observers_) {
    observer.OnMediaItemUISizeChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions for testing:

views::ImageView* MediaItemUIUpdatedView::GetArtworkViewForTesting() {
  return artwork_view_;
}

views::ImageView* MediaItemUIUpdatedView::GetFaviconViewForTesting() {
  return favicon_view_;
}

views::ImageView* MediaItemUIUpdatedView::GetCastingIndicatorViewForTesting() {
  return casting_indicator_view_;
}

views::Label* MediaItemUIUpdatedView::GetSourceLabelForTesting() {
  return source_label_;
}

views::Label* MediaItemUIUpdatedView::GetTitleLabelForTesting() {
  return title_label_;
}

views::Label* MediaItemUIUpdatedView::GetArtistLabelForTesting() {
  return artist_label_;
}

views::Label* MediaItemUIUpdatedView::GetCurrentTimestampLabelForTesting() {
  return current_timestamp_label_;
}

views::Label* MediaItemUIUpdatedView::GetDurationTimestampLabelForTesting() {
  return duration_timestamp_label_;
}

MediaActionButton* MediaItemUIUpdatedView::GetMediaActionButtonForTesting(
    MediaSessionAction action) {
  const auto i = base::ranges::find(
      media_action_buttons_, static_cast<int>(action), &views::View::GetID);
  return (i == media_action_buttons_.end()) ? nullptr : *i;
}

MediaProgressView* MediaItemUIUpdatedView::GetProgressViewForTesting() {
  return progress_view_;
}

MediaLiveStatusView* MediaItemUIUpdatedView::GetLiveStatusViewForTesting() {
  return live_status_view_;
}

MediaActionButton* MediaItemUIUpdatedView::GetStartCastingButtonForTesting() {
  return start_casting_button_;
}

MediaItemUIDeviceSelector*
MediaItemUIUpdatedView::GetDeviceSelectorForTesting() {
  return device_selector_view_;
}

MediaItemUIFooter* MediaItemUIUpdatedView::GetFooterForTesting() {
  return footer_view_;
}

BEGIN_METADATA(MediaItemUIUpdatedView)
END_METADATA

}  // namespace global_media_controls
