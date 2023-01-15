// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_ash_impl.h"

#include "components/media_message_center/media_artwork_view.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

// Dimensions.
constexpr auto kBorderInsets = gfx::Insets::TLBR(16, 8, 8, 8);
constexpr auto kMainRowInsets = gfx::Insets::VH(0, 8);
constexpr auto kInfoColumnInsets = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr auto kProgressViewInsets = gfx::Insets::VH(0, 14);
constexpr auto kTitleLabelInsets = gfx::Insets::TLBR(10, 0, 0, 0);

constexpr int kMainSeparator = 12;
constexpr int kMainRowSeparator = 8;
constexpr int kMediaInfoSeparator = 4;
constexpr int kPlayPauseContainerSeperator = 8;
constexpr int kPlayPauseIconSize = 26;
constexpr int kControlsIconSize = 20;
constexpr int kArtworkCornerRadius = 12;
constexpr int kSourceLineHeight = 18;
constexpr int kTitleArtistLineHeight = 20;

constexpr auto kArtworkSize = gfx::Size(80, 80);
constexpr auto kPlayPauseButtonSize = gfx::Size(48, 48);
constexpr auto kControlsButtonSize = gfx::Size(32, 32);

// TODO(jazzhsu): Make sure the media button style match the mock. 1. The play
// pause button should always have a background; 2. Figure out the hover effect
// for the rest of the controls.
class MediaButton : public views::ImageButton {
 public:
  MediaButton(PressedCallback callback, int icon_size, gfx::Size button_size)
      : ImageButton(callback), icon_size_(icon_size) {
    SetHasInkDropActionOnClick(true);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  button_size.height() / 2);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
        &MediaButton::GetForegroundColor, base::Unretained(this)));
    SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetFlipCanvasOnPaintForRTLUI(false);
    SetPreferredSize(button_size);
  }

  void SetButtonColor(SkColor foreground_color,
                      SkColor foreground_disabled_color) {
    foreground_color_ = foreground_color;
    foreground_disabled_color_ = foreground_disabled_color;

    views::SetImageFromVectorIconWithColor(
        this, *GetVectorIconForMediaAction(GetActionFromButtonTag(*this)),
        icon_size_, foreground_color_, foreground_disabled_color_);

    SchedulePaint();
  }

  void set_tag(int tag) {
    views::ImageButton::set_tag(tag);

    SetTooltipText(
        GetAccessibleNameForMediaAction(GetActionFromButtonTag(*this)));
    SetAccessibleName(
        GetAccessibleNameForMediaAction(GetActionFromButtonTag(*this)));
    views::SetImageFromVectorIconWithColor(
        this, *GetVectorIconForMediaAction(GetActionFromButtonTag(*this)),
        icon_size_, foreground_color_, foreground_disabled_color_);
  }

 private:
  SkColor GetForegroundColor() { return foreground_color_; }

  SkColor foreground_color_ = gfx::kPlaceholderColor;
  SkColor foreground_disabled_color_ = gfx::kPlaceholderColor;
  int icon_size_;
};

}  // namespace

MediaNotificationViewAshImpl::MediaNotificationViewAshImpl(
    MediaNotificationContainer* container,
    base::WeakPtr<MediaNotificationItem> item,
    std::unique_ptr<views::View> dismiss_button,
    absl::optional<NotificationTheme> theme)
    : container_(container), item_(std::move(item)), theme_(theme) {
  DCHECK(container_);
  DCHECK(dismiss_button);
  DCHECK(item_);

  // We should always have a theme passing from CrOS.
  DCHECK(theme_.has_value());

  // TODO(jazzhsu): Replace this with actual background color from |theme_|
  SkColor background_color = SK_ColorTRANSPARENT;

  SetBorder(views::CreateEmptyBorder(kBorderInsets));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kMainSeparator));

  // |main_row| holds all the media object's information, as well as the
  // play/pause button.
  auto* main_row = AddChildView(std::make_unique<views::View>());
  auto* main_row_layout =
      main_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMainRowInsets,
          kMainRowSeparator));

  // TODO(crbug.com/1406718): This is a temporary placeholder for artwork
  // until we figure out the correct way for displaying artwork.
  artwork_view_ = main_row->AddChildView(std::make_unique<MediaArtworkView>(
      kArtworkCornerRadius, kArtworkSize, gfx::Size()));
  artwork_view_->SetPreferredSize(kArtworkSize);
  artwork_view_->SetVignetteColor(background_color);
  artwork_view_->SetBackgroundColor(theme_->disabled_icon_color);

  // |media_info_column| holds the source, title, and artist.
  auto* media_info_column =
      main_row->AddChildView(std::make_unique<views::View>());
  media_info_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kInfoColumnInsets,
      kMediaInfoSeparator));
  main_row_layout->SetFlexForView(media_info_column, 1);

  source_label_ =
      media_info_column->AddChildView(std::make_unique<views::Label>(
          base::EmptyString16(), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
  source_label_->SetLineHeight(kSourceLineHeight);
  source_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  source_label_->SetEnabledColor(theme_->secondary_text_color);

  title_label_ = media_info_column->AddChildView(std::make_unique<views::Label>(
      base::EmptyString16(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_label_->SetLineHeight(kTitleArtistLineHeight);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetEnabledColor(theme_->primary_text_color);
  title_label_->SetProperty(views::kMarginsKey, kTitleLabelInsets);

  artist_label_ =
      media_info_column->AddChildView(std::make_unique<views::Label>(
          base::EmptyString16(), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
  artist_label_->SetLineHeight(kTitleArtistLineHeight);
  artist_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  artist_label_->SetEnabledColor(theme_->secondary_text_color);

  // |play_payse_container| holds the play/pause button and dismiss button.
  auto* play_pause_container =
      main_row->AddChildView(std::make_unique<views::View>());
  play_pause_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kPlayPauseContainerSeperator))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);

  play_pause_container->AddChildView(std::move(dismiss_button));
  play_pause_button_ =
      CreateMediaButton(play_pause_container, MediaSessionAction::kPlay);

  // |controls_row| holds all available media action buttons and the progress
  // bar.
  auto* controls_row = AddChildView(std::make_unique<views::View>());
  auto* controls_row_layout =
      controls_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
  controls_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  CreateMediaButton(controls_row, MediaSessionAction::kPreviousTrack);
  progress_view_ =
      controls_row->AddChildView(std::make_unique<MediaControlsProgressView>(
          base::BindRepeating(&MediaNotificationViewAshImpl::SeekTo,
                              base::Unretained(this)),
          /*is_modern_notification=*/true));
  progress_view_->SetForegroundColor(theme_->enabled_icon_color);
  progress_view_->SetBackgroundColor(theme_->disabled_icon_color);
  progress_view_->SetProperty(views::kMarginsKey, kProgressViewInsets);
  controls_row_layout->SetFlexForView(progress_view_, 1);

  CreateMediaButton(controls_row, MediaSessionAction::kNextTrack);
  picture_in_picture_button_ = CreateMediaButton(
      controls_row, MediaSessionAction::kEnterPictureInPicture);

  container_->OnColorsChanged(theme_->enabled_icon_color,
                              theme_->disabled_icon_color, background_color);
  item_->SetView(this);
}

MediaNotificationViewAshImpl::~MediaNotificationViewAshImpl() {
  item_->SetView(nullptr);
}

MediaButton* MediaNotificationViewAshImpl::CreateMediaButton(
    views::View* parent,
    MediaSessionAction action) {
  auto button = std::make_unique<MediaButton>(
      views::Button::PressedCallback(),
      action == MediaSessionAction::kPlay ? kPlayPauseIconSize
                                          : kControlsIconSize,
      action == MediaSessionAction::kPlay ? kPlayPauseButtonSize
                                          : kControlsButtonSize);
  button->SetCallback(
      base::BindRepeating(&MediaNotificationViewAshImpl::ButtonPressed,
                          base::Unretained(this), button.get()));
  button->set_tag(static_cast<int>(action));
  button->SetButtonColor(theme_->enabled_icon_color,
                         theme_->disabled_icon_color);
  auto* button_ptr = parent->AddChildView(std::move(button));
  action_buttons_.push_back(button_ptr);

  return button_ptr;
}

void MediaNotificationViewAshImpl::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  play_pause_button_->set_tag(static_cast<int>(
      playing ? MediaSessionAction::kPause : MediaSessionAction::kPlay));

  bool in_picture_in_picture =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  picture_in_picture_button_->set_tag(static_cast<int>(
      in_picture_in_picture ? MediaSessionAction::kExitPictureInPicture
                            : MediaSessionAction::kEnterPictureInPicture));

  UpdateActionButtonsVisibility();
  container_->OnMediaSessionInfoChanged(session_info);
}

void MediaNotificationViewAshImpl::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
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
  progress_view_->UpdateProgress(position);
}

void MediaNotificationViewAshImpl::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  artwork_view_->SetImage(image);
  SchedulePaint();
}

void MediaNotificationViewAshImpl::UpdateActionButtonsVisibility() {
  bool should_invalidate_layout = false;
  for (auto* button : action_buttons_) {
    bool should_show =
        base::Contains(enabled_actions_, GetActionFromButtonTag(*button));
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
  item_->OnMediaSessionActionButtonPressed(GetActionFromButtonTag(*button));
}

void MediaNotificationViewAshImpl::SeekTo(double seek_progress) {
  item_->SeekTo(seek_progress * position_.duration());
}

BEGIN_METADATA(MediaNotificationViewAshImpl, views::View)
END_METADATA

}  // namespace media_message_center
