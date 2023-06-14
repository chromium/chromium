// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_ash_impl.h"

#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_squiggly_progress_view.h"
#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
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
#include "ui/views/view_class_properties.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

// Dimensions.
constexpr auto kBorderInsets = gfx::Insets::TLBR(16, 8, 8, 8);
constexpr auto kMainRowInsets = gfx::Insets::VH(0, 8);
constexpr auto kInfoColumnInsets = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr auto kPlayPauseContainerInsets = gfx::Insets::VH(8, 0);
constexpr auto kSourceLabelInsets = gfx::Insets::TLBR(0, 0, 10, 0);

constexpr int kMainSeparator = 12;
constexpr int kMainRowSeparator = 8;
constexpr int kMediaInfoSeparator = 4;
constexpr int kControlsRowSeparator = 2;
constexpr int kChevronIconSize = 15;
constexpr int kPlayPauseIconSize = 26;
constexpr int kControlsIconSize = 20;
constexpr int kBackgroundCornerRadius = 12;
constexpr int kArtworkCornerRadius = 10;
constexpr int kSourceLineHeight = 18;
constexpr int kTitleArtistLineHeight = 20;
constexpr int kNotMediaActionButtonId = -1;

constexpr auto kArtworkSize = gfx::Size(80, 80);
constexpr auto kPlayPauseButtonSize = gfx::Size(48, 48);
constexpr auto kControlsButtonSize = gfx::Size(32, 32);

class MediaButton : public views::ImageButton {
 public:
  MediaButton(PressedCallback callback,
              int button_id,
              const gfx::VectorIcon& vector_icon,
              int tooltip_text_id,
              ui::ColorId foreground_color_id,
              ui::ColorId foreground_disabled_color_id)
      : ImageButton(std::move(callback)),
        icon_size_(button_id == static_cast<int>(MediaSessionAction::kPlay)
                       ? kPlayPauseIconSize
                       : kControlsIconSize),
        foreground_color_id_(foreground_color_id),
        foreground_disabled_color_id_(foreground_disabled_color_id) {
    views::ConfigureVectorImageButton(this);
    SetInstallFocusRingOnFocus(true);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetFlipCanvasOnPaintForRTLUI(false);

    auto button_size = (button_id == static_cast<int>(MediaSessionAction::kPlay)
                            ? kPlayPauseButtonSize
                            : kControlsButtonSize);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  button_size.height() / 2);
    SetPreferredSize(button_size);

    Update(button_id, vector_icon, tooltip_text_id);
  }

  void Update(int button_id,
              const gfx::VectorIcon& vector_icon,
              int tooltip_text_id) {
    if (button_id != kNotMediaActionButtonId) {
      SetID(button_id);
    }
    SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
    views::SetImageFromVectorIconWithColorId(
        this, vector_icon, foreground_color_id_, foreground_disabled_color_id_,
        icon_size_);
  }

 private:
  const int icon_size_;
  const ui::ColorId foreground_color_id_;
  const ui::ColorId foreground_disabled_color_id_;
};

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
    MediaNotificationContainer* container,
    base::WeakPtr<MediaNotificationItem> item,
    MediaColorTheme theme,
    MediaDisplayPage media_display_page)
    : container_(container), item_(std::move(item)), theme_(theme) {
  DCHECK(container_);
  DCHECK(item_);

  SetBorder(views::CreateEmptyBorder(kBorderInsets));
  SetBackground(views::CreateThemedRoundedRectBackground(
      theme_.background_color_id, kBackgroundCornerRadius));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kMainSeparator));

  // |main_row| holds all the media object's information, as well as the
  // play/pause button.
  auto* main_row = AddChildView(std::make_unique<views::View>());
  auto* main_row_layout =
      main_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMainRowInsets,
          kMainRowSeparator));

  artwork_view_ = main_row->AddChildView(std::make_unique<views::ImageView>());
  artwork_view_->SetPreferredSize(kArtworkSize);

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
  source_label_->SetEnabledColorId(theme_.secondary_foreground_color_id);
  source_label_->SetProperty(views::kMarginsKey, kSourceLabelInsets);

  title_row_ =
      media_info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  title_row_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  title_label_ = title_row_->AddChildView(std::make_unique<views::Label>(
      base::EmptyString16(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_label_->SetLineHeight(kTitleArtistLineHeight);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetEnabledColorId(theme_.primary_foreground_color_id);
  title_row_->SetFlexForView(title_label_, 1);

  // Add a chevron right icon to the title if the media is displaying on the
  // quick settings media view to indicate user can click on the view to go to
  // the detailed view page.
  if (media_display_page == MediaDisplayPage::kQuickSettingsMediaView) {
    chevron_icon_ = title_row_->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kChevronRightIcon, theme_.secondary_foreground_color_id,
            kChevronIconSize)));
  }

  artist_label_ =
      media_info_column->AddChildView(std::make_unique<views::Label>(
          base::EmptyString16(), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
  artist_label_->SetLineHeight(kTitleArtistLineHeight);
  artist_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  artist_label_->SetEnabledColorId(theme_.secondary_foreground_color_id);

  // Create the play/pause button.
  auto* play_pause_container =
      main_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  play_pause_container->SetInsideBorderInsets(kPlayPauseContainerInsets);
  play_pause_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);

  play_pause_button_ = CreateMediaButton(
      play_pause_container, static_cast<int>(MediaSessionAction::kPlay),
      kPlayArrowIcon, IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY);
  play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      theme_.secondary_container_color_id, kPlayPauseButtonSize.height() / 2));

  // |controls_row| holds all the available media action buttons and the
  // progress view.
  auto* controls_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  controls_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  controls_row->SetBetweenChildSpacing(kControlsRowSeparator);

  // Create the previous track button.
  CreateMediaButton(
      controls_row, static_cast<int>(MediaSessionAction::kPreviousTrack),
      kMediaPreviousTrackIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK);

  // Create the squiggly progress view.
  squiggly_progress_view_ =
      controls_row->AddChildView(std::make_unique<MediaSquigglyProgressView>(
          theme_.primary_container_color_id,
          theme_.secondary_container_color_id,
          base::BindRepeating(&MediaNotificationViewAshImpl::SeekTo,
                              base::Unretained(this))));
  controls_row->SetFlexForView(squiggly_progress_view_, 1);

  // Create the next track button.
  CreateMediaButton(
      controls_row, static_cast<int>(MediaSessionAction::kNextTrack),
      kMediaNextTrackIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK);

  // Create the start casting button.
  start_casting_button_ = CreateMediaButton(
      controls_row, kNotMediaActionButtonId, kMediaCastIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_START_CASTING);

  // Create the picture in picture button.
  picture_in_picture_button_ = CreateMediaButton(
      controls_row,
      static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
      kMediaEnterPipIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);

  item_->SetView(this);
}

MediaNotificationViewAshImpl::~MediaNotificationViewAshImpl() {
  if (item_) {
    item_->SetView(nullptr);
  }
}

MediaButton* MediaNotificationViewAshImpl::CreateMediaButton(
    views::View* parent,
    int button_id,
    const gfx::VectorIcon& vector_icon,
    int tooltip_text_id) {
  auto button = std::make_unique<MediaButton>(
      views::Button::PressedCallback(), button_id, vector_icon, tooltip_text_id,
      theme_.primary_foreground_color_id, theme_.secondary_foreground_color_id);
  auto* button_ptr = parent->AddChildView(std::move(button));

  if (button_id != kNotMediaActionButtonId) {
    button_ptr->SetCallback(
        base::BindRepeating(&MediaNotificationViewAshImpl::ButtonPressed,
                            base::Unretained(this), button_ptr));
    action_buttons_.push_back(button_ptr);
  }
  return button_ptr;
}

void MediaNotificationViewAshImpl::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  if (playing) {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPause), kPauseIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE);
  } else {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPlay), kPlayArrowIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY);
  }

  bool in_picture_in_picture =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  if (in_picture_in_picture) {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kExitPictureInPicture),
        kMediaExitPipIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP);
  } else {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
        kMediaEnterPipIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);
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

void MediaNotificationViewAshImpl::UpdateActionButtonsVisibility() {
  bool should_invalidate_layout = false;
  for (auto* button : action_buttons_) {
    bool should_show = base::Contains(
        enabled_actions_, static_cast<MediaSessionAction>(button->GetID()));
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
  item_->OnMediaSessionActionButtonPressed(
      static_cast<MediaSessionAction>(button->GetID()));
}

void MediaNotificationViewAshImpl::SeekTo(double seek_progress) {
  item_->SeekTo(seek_progress * position_.duration());
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

BEGIN_METADATA(MediaNotificationViewAshImpl, views::View)
END_METADATA

}  // namespace media_message_center
