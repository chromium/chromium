// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"

#include "components/global_media_controls/media_view_utils.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
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

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::VH(16, 16);
constexpr gfx::Insets kInfoColumnInsets = gfx::Insets::TLBR(4, 0, 0, 0);

constexpr int kBackgroundCornerRadius = 8;
constexpr int kArtworkCornerRadius = 8;

constexpr int kBackgroundSeparator = 16;
constexpr int kArtworkRowSeparator = 12;
constexpr int kMediaInfoSeparator = 8;
constexpr int kSourceRowSeparator = 16;
constexpr int kSourceRowButtonContainerSeparator = 4;
constexpr int kMetadataRowSeparator = 16;
constexpr int kMetadataColumnSeparator = 4;
constexpr int kProgressRowSeparator = 4;

constexpr int kPlayPauseButtonIconSize = 24;
constexpr int kMediaActionButtonIconSize = 20;

constexpr float kFocusRingHaloInset = -3.0f;

constexpr gfx::Size kArtworkSize = gfx::Size(80, 80);
constexpr gfx::Size kPlayPauseButtonSize = gfx::Size(48, 48);
constexpr gfx::Size kMediaActionButtonSize = gfx::Size(24, 24);

// Buttons with the following media actions should be hidden when the user is
// dragging the progress view.
const MediaSessionAction kHiddenMediaActionsWhileDragging[] = {
    MediaSessionAction::kPreviousTrack, MediaSessionAction::kNextTrack,
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
  artwork_row->SetBetweenChildSpacing(kArtworkRowSeparator);

  artwork_view_ =
      artwork_row->AddChildView(std::make_unique<views::ImageView>());
  artwork_view_->SetPreferredSize(kArtworkSize);
  artwork_view_->SetVisible(false);

  // |info_column| inside |artwork_row| right to the |artwork_view| holds the
  // |source_row| and |metadata_row|.
  auto* info_column =
      artwork_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  info_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  info_column->SetInsideBorderInsets(kInfoColumnInsets);
  info_column->SetBetweenChildSpacing(kMediaInfoSeparator);
  artwork_row->SetFlexForView(info_column, 1);

  // |source_row| inside |info_column| holds the |source_label_container| and
  // |source_row_button_container|.
  auto* source_row =
      info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row->SetBetweenChildSpacing(kSourceRowSeparator);
  auto* source_label_container =
      source_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row->SetFlexForView(source_label_container, 1);

  // |source_label_container| inside |source_row| holds the media source label.
  source_label_ =
      source_label_container->AddChildView(std::make_unique<views::Label>(
          std::u16string(), views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_5));
  source_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  source_label_->SetElideBehavior(gfx::ELIDE_HEAD);

  // |source_row_button_container| inside |source_row| holds the start casting
  // button and picture-in-picture button.
  auto* source_row_button_container =
      source_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row_button_container->SetBetweenChildSpacing(
      kSourceRowButtonContainerSeparator);

  // Create the start casting button.
  start_casting_button_ = CreateMediaActionButton(
      source_row_button_container, kEmptyMediaActionButtonId,
      vector_icons::kCastIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
  start_casting_button_->SetVisible(false);

  // Create the picture-in-picture button.
  picture_in_picture_button_ = CreateMediaActionButton(
      source_row_button_container,
      static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
      vector_icons::kPictureInPictureAltIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);

  // |metadata_row| inside |info_column| holds the |metadata_column| and
  // |play_pause_button_container|.
  auto* metadata_row =
      info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  metadata_row->SetBetweenChildSpacing(kMetadataRowSeparator);
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
  artist_label_ = metadata_column->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_2));
  artist_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

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
  progress_row->SetBetweenChildSpacing(kProgressRowSeparator);

  // Create the current timestamp label before the progress view.
  current_timestamp_label_ =
      progress_row->AddChildView(std::make_unique<views::Label>(
          std::u16string(), views::style::CONTEXT_LABEL,
          views::style::STYLE_CAPTION_MEDIUM));

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
          views::style::STYLE_CAPTION_MEDIUM));

  // Add the device selector view below the |progress_row| if there is one.
  UpdateDeviceSelectorView(std::move(device_selector_view));

  // Add the cast device footer view below the |progress_row| if there is one.
  // It will only show up when this media item is being casted to another
  // device.
  UpdateFooterView(std::move(footer_view));

  // Set the timestamp labels to be hidden initially.
  UpdateTimestampLabelsVisibility();

  item_->SetView(this);
}

MediaItemUIUpdatedView::~MediaItemUIUpdatedView() {
  if (item_) {
    item_->SetView(nullptr);
  }
  for (auto& observer : observers_) {
    observer.OnMediaItemUIDestroyed(id_);
  }
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

void MediaItemUIUpdatedView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kListItem;
  node_data->SetNameChecked(l10n_util::GetStringUTF8(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
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

    // Draw the image with rounded corners.
    auto path = SkPath().addRoundRect(
        RectToSkRect(gfx::Rect(kArtworkSize.width(), kArtworkSize.height())),
        kArtworkCornerRadius, kArtworkCornerRadius);
    artwork_view_->SetClipPath(path);
  }
  SchedulePaint();
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
    RemoveChildViewT(device_selector_view_);
    device_selector_view_ = nullptr;
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
    RemoveChildViewT(footer_view_);
    footer_view_ = nullptr;
  }
  // Add the new footer view.
  if (footer_view) {
    footer_view_ = AddChildView(std::move(footer_view));
  }
  // Footer view changes can change the picture-in-picture button's visibility.
  UpdateMediaActionButtonsVisibility();
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
      media_color_theme_.secondary_foreground_color_id,
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
    if (drag_state_ == DragState::kDragStarted &&
        base::Contains(kHiddenMediaActionsWhileDragging,
                       static_cast<MediaSessionAction>(button->GetID()))) {
      should_show = false;
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
  } else {
    device_selector_view_->ShowDevices();
  }
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
