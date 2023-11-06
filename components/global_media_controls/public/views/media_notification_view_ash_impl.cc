// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_notification_view_ash_impl.h"

#include "base/metrics/histogram_functions.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_squiggly_progress_view.h"
#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace global_media_controls {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::TLBR(16, 8, 8, 8);
constexpr gfx::Insets kMainRowInsets = gfx::Insets::TLBR(0, 8, 8, 8);
constexpr gfx::Insets kMediaInfoInsets = gfx::Insets::TLBR(0, 16, 0, 4);
constexpr gfx::Insets kSourceRowInsets = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr gfx::Insets kControlsColumnInsets = gfx::Insets::TLBR(0, 2, 8, 2);
constexpr gfx::Insets kDeviceSelectorSeparatorInsets = gfx::Insets::VH(10, 12);
constexpr gfx::Insets kDeviceSelectorSeparatorLineInsets =
    gfx::Insets::VH(1, 1);

constexpr int kBackgroundCornerRadius = 16;
constexpr int kArtworkCornerRadius = 12;
constexpr int kTextLineHeight = 20;
constexpr int kFontSize = 12;
constexpr int kMediaInfoSeparator = 4;
constexpr int kControlsColumnSeparator = 8;
constexpr int kChevronIconSize = 15;
constexpr int kPlayPauseIconSize = 26;
constexpr int kControlsIconSize = 20;
constexpr int kNotMediaActionButtonId = -1;

constexpr float kFocusRingHaloInset = -3.0f;

constexpr gfx::Size kArtworkSize = gfx::Size(80, 80);
constexpr gfx::Size kPlayPauseButtonSize = gfx::Size(48, 48);
constexpr gfx::Size kControlsButtonSize = gfx::Size(32, 32);

constexpr char kMediaDisplayPageHistogram[] = "Media.Notification.DisplayPage";

class MediaButton : public views::ImageButton {
 public:
  METADATA_HEADER(MediaButton);
  MediaButton(PressedCallback callback,
              int button_id,
              const gfx::VectorIcon& vector_icon,
              int tooltip_text_id,
              ui::ColorId foreground_color_id,
              ui::ColorId foreground_disabled_color_id,
              ui::ColorId focus_ring_color_id)
      : ImageButton(std::move(callback)),
        icon_size_(button_id == static_cast<int>(MediaSessionAction::kPlay)
                       ? kPlayPauseIconSize
                       : kControlsIconSize),
        foreground_disabled_color_id_(foreground_disabled_color_id) {
    views::ConfigureVectorImageButton(this);
    SetFlipCanvasOnPaintForRTLUI(false);

    auto button_size = (button_id == static_cast<int>(MediaSessionAction::kPlay)
                            ? kPlayPauseButtonSize
                            : kControlsButtonSize);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  button_size.height() / 2);
    SetPreferredSize(button_size);

    SetInstallFocusRingOnFocus(true);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    views::FocusRing::Get(this)->SetColorId(focus_ring_color_id);

    Update(button_id, vector_icon, tooltip_text_id, foreground_color_id);
  }

  void Update(int button_id,
              const gfx::VectorIcon& vector_icon,
              int tooltip_text_id,
              ui::ColorId foreground_color_id) {
    if (button_id != kNotMediaActionButtonId) {
      SetID(button_id);
    }
    SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
    views::SetImageFromVectorIconWithColorId(
        this, vector_icon, foreground_color_id, foreground_disabled_color_id_,
        icon_size_);
  }

  void UpdateText(int tooltip_text_id) {
    SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  }

 private:
  const int icon_size_;
  const ui::ColorId foreground_disabled_color_id_;
};

BEGIN_METADATA(MediaButton, views::ImageButton)
END_METADATA

// If the image does not fit the square view, scale the image to fill the view
// even if part of the image is cropped.
gfx::Size ScaleImageSizeToFitView(const gfx::Size& image_size,
                                  const gfx::Size& view_size) {
  const float scale =
      std::max(view_size.width() / static_cast<float>(image_size.width()),
               view_size.height() / static_cast<float>(image_size.height()));
  return gfx::ScaleToFlooredSize(image_size, scale);
}

}  // namespace

MediaNotificationViewAshImpl::MediaNotificationViewAshImpl(
    media_message_center::MediaNotificationContainer* container,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    std::unique_ptr<MediaItemUIFooter> footer_view,
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
    std::unique_ptr<views::View> dismiss_button,
    media_message_center::MediaColorTheme theme,
    MediaDisplayPage media_display_page)
    : container_(container),
      item_(std::move(item)),
      theme_(theme),
      media_display_page_(media_display_page) {
  CHECK(container_);

  // Media display page histogram for lock screen media view will be recorded in
  // LockScreenMediaView when the media view becomes visible to users.
  if (media_display_page_ == MediaDisplayPage::kLockScreenMediaView) {
    CHECK(dismiss_button);
  } else {
    CHECK(item_);
    base::UmaHistogramEnumeration(kMediaDisplayPageHistogram,
                                  media_display_page_);
  }

  SetBackground(views::CreateThemedRoundedRectBackground(
      theme_.background_color_id, kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets));

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kBackgroundCornerRadius);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetColorId(theme_.focus_ring_color_id);

  // |main_row| holds the media artwork, media information column and the
  // play/pause button.
  auto* main_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  main_row->SetInsideBorderInsets(kMainRowInsets);

  artwork_view_ = main_row->AddChildView(std::make_unique<views::ImageView>());
  artwork_view_->SetPreferredSize(kArtworkSize);

  // |media_info_column| inside |main_row| holds the media source, title, and
  // artist.
  auto* media_info_column =
      main_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  media_info_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  media_info_column->SetInsideBorderInsets(kMediaInfoInsets);
  media_info_column->SetBetweenChildSpacing(kMediaInfoSeparator);
  main_row->SetFlexForView(media_info_column, 1);

  const views::Label::CustomFont text_fonts = {
      gfx::FontList({"Google Sans", "Roboto"}, gfx::Font::NORMAL, kFontSize,
                    gfx::Font::Weight::NORMAL)};

  // Create the media source label.
  auto* source_row =
      media_info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row->SetInsideBorderInsets(kSourceRowInsets);

  source_label_ = source_row->AddChildView(
      std::make_unique<views::Label>(base::EmptyString16(), text_fonts));
  source_label_->SetLineHeight(kTextLineHeight);
  source_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  source_label_->SetEnabledColorId(theme_.secondary_foreground_color_id);
  source_row->SetFlexForView(source_label_, 1);

  // Create the media title label.
  auto* title_row =
      media_info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  title_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  title_label_ = title_row->AddChildView(
      std::make_unique<views::Label>(base::EmptyString16(), text_fonts));
  title_label_->SetLineHeight(kTextLineHeight);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetEnabledColorId(theme_.primary_foreground_color_id);
  title_row->SetFlexForView(title_label_, 1);

  // Add a chevron right icon to the title if the media is displaying on the
  // quick settings media view to indicate user can click on the view to go to
  // the detailed view page.
  if (media_display_page_ == MediaDisplayPage::kQuickSettingsMediaView) {
    chevron_icon_ = title_row->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            media_message_center::kChevronRightIcon,
            theme_.secondary_foreground_color_id, kChevronIconSize)));
  }

  // Create the media artist label.
  artist_label_ = media_info_column->AddChildView(
      std::make_unique<views::Label>(base::EmptyString16(), text_fonts));
  artist_label_->SetLineHeight(kTextLineHeight);
  artist_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  artist_label_->SetEnabledColorId(theme_.secondary_foreground_color_id);

  // |controls_column| inside |main_row| holds the play/pause button and the
  // dismiss button on the top right corner if it exists.
  auto* controls_column =
      main_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  controls_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  controls_column->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  controls_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  controls_column->SetInsideBorderInsets(kControlsColumnInsets);

  if (dismiss_button) {
    controls_column->SetBetweenChildSpacing(kControlsColumnSeparator);
    controls_column->AddChildView(std::move(dismiss_button));
  }

  // Create the play/pause button.
  play_pause_button_ = CreateMediaButton(
      controls_column, static_cast<int>(MediaSessionAction::kPlay),
      media_message_center::kPlayArrowIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY);
  play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      theme_.play_button_container_color_id,
      kPlayPauseButtonSize.height() / 2));

  // |controls_row| holds all the available media action buttons and the
  // progress view.
  auto* controls_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  controls_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Create the previous track button.
  CreateMediaButton(
      controls_row, static_cast<int>(MediaSessionAction::kPreviousTrack),
      media_message_center::kMediaPreviousTrackIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK);

  // Create the squiggly progress view.
  squiggly_progress_view_ = controls_row->AddChildView(
      std::make_unique<media_message_center::MediaSquigglyProgressView>(
          theme_.playing_progress_foreground_color_id,
          theme_.playing_progress_background_color_id,
          theme_.paused_progress_foreground_color_id,
          theme_.paused_progress_background_color_id,
          theme_.focus_ring_color_id,
          base::BindRepeating(&MediaNotificationViewAshImpl::OnProgressDragging,
                              base::Unretained(this)),
          base::BindRepeating(&MediaNotificationViewAshImpl::SeekTo,
                              base::Unretained(this))));
  controls_row->SetFlexForView(squiggly_progress_view_, 1);

  // Create the next track button.
  CreateMediaButton(
      controls_row, static_cast<int>(MediaSessionAction::kNextTrack),
      media_message_center::kMediaNextTrackIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK);

  // Create the start casting button.
  if (device_selector_view) {
    start_casting_button_ = CreateMediaButton(
        controls_row, kNotMediaActionButtonId,
        media_message_center::kMediaCastStartIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
    start_casting_button_->SetCallback(base::BindRepeating(
        &MediaNotificationViewAshImpl::StartCastingButtonPressed,
        base::Unretained(this)));
    start_casting_button_->SetVisible(false);
  }

  // Create the picture-in-picture button.
  picture_in_picture_button_ = CreateMediaButton(
      controls_row,
      static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
      media_message_center::kMediaEnterPipIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);

  // Create the stop casting button. It will only show up when this media item
  // is being casted to another device.
  if (footer_view) {
    footer_view_ = controls_row->AddChildView(std::move(footer_view));
    picture_in_picture_button_->SetVisible(false);
  }

  if (device_selector_view) {
    // Create a separator line between the media view and device selector view.
    device_selector_view_separator_ =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    device_selector_view_separator_->SetInsideBorderInsets(
        kDeviceSelectorSeparatorInsets);
    auto* separator = device_selector_view_separator_->AddChildView(
        std::make_unique<views::BoxLayoutView>());
    separator->SetInsideBorderInsets(kDeviceSelectorSeparatorLineInsets);
    separator->SetBackground(
        views::CreateThemedSolidBackground(theme_.separator_color_id));
    device_selector_view_separator_->SetFlexForView(separator, 1);

    // Create the device selector view.
    device_selector_view_ = AddChildView(std::move(device_selector_view));
  }

  if (item_) {
    item_->SetView(this);
  }
}

MediaNotificationViewAshImpl::~MediaNotificationViewAshImpl() {
  if (item_) {
    item_->SetView(nullptr);
  }
}

///////////////////////////////////////////////////////////////////////////////
// MediaNotificationView implementations:

void MediaNotificationViewAshImpl::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  if (playing) {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPause),
        media_message_center::kPauseIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE,
        theme_.pause_button_foreground_color_id);
    play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        theme_.pause_button_container_color_id,
        kPlayPauseButtonSize.height() / 2));
  } else {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPlay),
        media_message_center::kPlayArrowIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY,
        theme_.play_button_foreground_color_id);
    play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        theme_.play_button_container_color_id,
        kPlayPauseButtonSize.height() / 2));
  }

  in_picture_in_picture_ =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  if (in_picture_in_picture_) {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kExitPictureInPicture),
        media_message_center::kMediaExitPipIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP,
        theme_.primary_foreground_color_id);
  } else {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
        media_message_center::kMediaEnterPipIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP,
        theme_.primary_foreground_color_id);
  }

  UpdateActionButtonsVisibility();
  container_->OnMediaSessionInfoChanged(session_info);
}

void MediaNotificationViewAshImpl::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  source_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  source_label_->SetText(metadata.source_title);
  title_label_->SetText(metadata.title);
  artist_label_->SetText(metadata.artist);

  container_->OnMediaSessionMetadataChanged(metadata);
}

void MediaNotificationViewAshImpl::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;
  UpdateActionButtonsVisibility();

  container_->OnVisibleActionsChanged(enabled_actions_);
}

void MediaNotificationViewAshImpl::UpdateWithMediaPosition(
    const media_session::MediaPosition& position) {
  position_ = position;
  squiggly_progress_view_->UpdateProgress(position);
}

void MediaNotificationViewAshImpl::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Hide the image so the other contents will adjust to fill the container.
    artwork_view_->SetVisible(false);
  } else {
    artwork_view_->SetVisible(true);
    artwork_view_->SetImageSize(
        ScaleImageSizeToFitView(image.size(), kArtworkSize));
    artwork_view_->SetImage(image);

    // Draw the image with rounded corners.
    auto path = SkPath().addRoundRect(
        RectToSkRect(gfx::Rect(kArtworkSize.width(), kArtworkSize.height())),
        kArtworkCornerRadius, kArtworkCornerRadius);
    artwork_view_->SetClipPath(path);
  }
  SchedulePaint();
}

void MediaNotificationViewAshImpl::UpdateDeviceSelectorAvailability(
    bool has_devices) {
  CHECK(start_casting_button_);
  // Do not show the start casting button if this media item is being casted to
  // another device and has a footer view of stop casting button.
  bool visible = has_devices && !footer_view_;
  if (visible != start_casting_button_->GetVisible()) {
    start_casting_button_->SetVisible(visible);
    UpdateCastingState();
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

void MediaNotificationViewAshImpl::AddedToWidget() {
  // Ink drop on the start casting button requires color provider to be ready,
  // so we need to update the state after the widget is ready.
  if (device_selector_view_) {
    UpdateCastingState();
  }
}

void MediaNotificationViewAshImpl::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kListItem;
  node_data->SetNameChecked(l10n_util::GetStringUTF8(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
}

///////////////////////////////////////////////////////////////////////////////
// MediaNotificationViewAshImpl implementations:

MediaButton* MediaNotificationViewAshImpl::CreateMediaButton(
    views::View* parent,
    int button_id,
    const gfx::VectorIcon& vector_icon,
    int tooltip_text_id) {
  auto button = std::make_unique<MediaButton>(
      views::Button::PressedCallback(), button_id, vector_icon, tooltip_text_id,
      theme_.primary_foreground_color_id, theme_.secondary_foreground_color_id,
      theme_.focus_ring_color_id);
  auto* button_ptr = parent->AddChildView(std::move(button));

  if (button_id != kNotMediaActionButtonId) {
    button_ptr->SetCallback(
        base::BindRepeating(&MediaNotificationViewAshImpl::ButtonPressed,
                            base::Unretained(this), button_ptr));
    action_buttons_.push_back(button_ptr);
  }
  return button_ptr;
}

void MediaNotificationViewAshImpl::UpdateActionButtonsVisibility() {
  bool should_invalidate_layout = false;

  for (auto* button : action_buttons_) {
    bool should_show = base::Contains(
        enabled_actions_, static_cast<MediaSessionAction>(button->GetID()));

    if (button == picture_in_picture_button_) {
      // Force the picture-in-picture button to be visible if the media is
      // currently in the picture-in-picture state, since the media actions
      // may not contain pip actions for a short period of time for unknown
      // reason, which can cause the picture-in-picture button to lose focus,
      // but we want the button to keep the focus so that the user is able to
      // undo the pip action immediately if needed.
      if (in_picture_in_picture_) {
        should_show = true;
      }

      // The picture-in-picture button remains invisible if there is a footer
      // view regardless of media actions.
      if (footer_view_) {
        should_show = false;
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

void MediaNotificationViewAshImpl::ButtonPressed(views::Button* button) {
  const auto action = static_cast<MediaSessionAction>(button->GetID());
  if (item_) {
    item_->OnMediaSessionActionButtonPressed(action);
  } else {
    // LockScreenMediaView does not have MediaNotificationItem and will handle
    // the action itself.
    container_->OnMediaSessionActionButtonPressed(action);
  }
}

void MediaNotificationViewAshImpl::OnProgressDragging(bool pause) {
  const auto action =
      (pause ? MediaSessionAction::kPause : MediaSessionAction::kPlay);
  if (item_) {
    item_->OnMediaSessionActionButtonPressed(action);
  } else {
    // LockScreenMediaView does not have MediaNotificationItem and will handle
    // the action itself.
    container_->OnMediaSessionActionButtonPressed(action);
  }
}

void MediaNotificationViewAshImpl::SeekTo(double seek_progress) {
  const auto time = seek_progress * position_.duration();
  if (item_) {
    item_->SeekTo(time);
  } else {
    // LockScreenMediaView does not have MediaNotificationItem and will handle
    // the seek event itself.
    container_->SeekTo(time);
  }
}

void MediaNotificationViewAshImpl::StartCastingButtonPressed() {
  CHECK(device_selector_view_);

  switch (media_display_page_) {
    case MediaDisplayPage::kQuickSettingsMediaView: {
      // Clicking the button on the quick settings media view should redirect
      // the user to the quick settings media detailed view and open the device
      // selector view there instead.
      container_->OnShowCastingDevicesRequested();
      break;
    }
    case MediaDisplayPage::kQuickSettingsMediaDetailedView:
    case MediaDisplayPage::kSystemShelfMediaDetailedView: {
      // Clicking the button on the media detailed view will open the device
      // selector view to show the device list.
      device_selector_view_->ShowOrHideDeviceList();
      UpdateCastingState();
      break;
    }
    default:
      NOTREACHED();
  }
}

void MediaNotificationViewAshImpl::UpdateCastingState() {
  CHECK(start_casting_button_);
  CHECK(device_selector_view_);
  CHECK(device_selector_view_separator_);

  if (!start_casting_button_->GetVisible()) {
    device_selector_view_->SetVisible(false);
    device_selector_view_separator_->SetVisible(false);
    return;
  }

  device_selector_view_->SetVisible(true);
  bool is_expanded = device_selector_view_->IsDeviceSelectorExpanded();
  if (is_expanded) {
    // Use the ink drop color as the button background if user clicks the button
    // to show devices.
    views::InkDrop::Get(start_casting_button_)->GetInkDrop()->SnapToActivated();

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
  device_selector_view_separator_->SetVisible(is_expanded);
}

// Helper functions for testing:
views::ImageView* MediaNotificationViewAshImpl::GetArtworkViewForTesting() {
  return artwork_view_;
}

views::Label* MediaNotificationViewAshImpl::GetSourceLabelForTesting() {
  return source_label_;
}

views::Label* MediaNotificationViewAshImpl::GetArtistLabelForTesting() {
  return artist_label_;
}

views::Label* MediaNotificationViewAshImpl::GetTitleLabelForTesting() {
  return title_label_;
}

views::ImageView* MediaNotificationViewAshImpl::GetChevronIconForTesting() {
  return chevron_icon_;
}

views::Button* MediaNotificationViewAshImpl::GetActionButtonForTesting(
    media_session::mojom::MediaSessionAction action) {
  const auto i = base::ranges::find(action_buttons_, static_cast<int>(action),
                                    &views::View::GetID);
  return (i == action_buttons_.end()) ? nullptr : *i;
}

media_session::MediaPosition
MediaNotificationViewAshImpl::GetPositionForTesting() {
  return position_;
}

views::Button* MediaNotificationViewAshImpl::GetStartCastingButtonForTesting() {
  return start_casting_button_;
}

MediaItemUIFooter* MediaNotificationViewAshImpl::GetFooterForTesting() {
  return footer_view_;
}

MediaItemUIDeviceSelector*
MediaNotificationViewAshImpl::GetDeviceSelectorForTesting() {
  return device_selector_view_;
}

views::View*
MediaNotificationViewAshImpl::GetDeviceSelectorSeparatorForTesting() {
  return device_selector_view_separator_;
}

BEGIN_METADATA(MediaNotificationViewAshImpl, views::View)
END_METADATA

}  // namespace global_media_controls
