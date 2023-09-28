// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_modern_impl.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "components/media_message_center/media_artwork_view.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "components/media_message_center/media_notification_background_ash_impl.h"
#include "components/media_message_center/media_notification_background_impl.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_notification_volume_slider_view.h"
#include "components/media_message_center/notification_theme.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr gfx::Size kMediaNotificationViewBaseSize = {350, 168};
constexpr gfx::Size kArtworkSize = {72, 72};
constexpr gfx::Size kInfoContainerSize = {
    kMediaNotificationViewBaseSize.width(), kArtworkSize.height()};
constexpr int kArtworkVignetteCornerRadius = 5;
constexpr gfx::Size kLabelsContainerBaseSize = {
    kMediaNotificationViewBaseSize.width() - kArtworkSize.width(),
    kInfoContainerSize.height()};
constexpr gfx::Size kPipButtonSize = {30, 20};
constexpr int kPipButtonIconSize = 16;
constexpr auto kNotificationControlsInsets = gfx::Insets::TLBR(8, 0, 0, 8);
constexpr gfx::Size kButtonsContainerSize = {
    kMediaNotificationViewBaseSize.width(), 40};
constexpr gfx::Size kMediaControlsContainerSize = {
    328 /* base width - dismissbutton size - margin*/, 32};
constexpr auto kMediaControlsContainerInsets = gfx::Insets::TLBR(0, 22, 0, 0);
constexpr int kMediaControlsButtonSpacing = 16;
constexpr auto kProgressBarInsets = gfx::Insets::TLBR(0, 16, 0, 16);
constexpr auto kInfoContainerInsets = gfx::Insets::TLBR(0, 15, 0, 16);
constexpr int kInfoContainerSpacing = 12;
constexpr gfx::Size kUtilButtonsContainerSize = {
    kMediaNotificationViewBaseSize.width(), 39};
constexpr auto kUtilButtonsContainerInsets = gfx::Insets::TLBR(7, 16, 12, 16);
constexpr int kUtilButtonsSpacing = 8;

constexpr int kTitleArtistLineHeight = 20;
constexpr gfx::Size kMediaButtonSize = {24, 24};
constexpr gfx::Size kPlayPauseButtonSize = {32, 32};
constexpr int kMediaButtonIconSize = 14;
constexpr int kPlayPauseIconSize = 20;
constexpr gfx::Size kFaviconSize = {20, 20};

constexpr gfx::Size kVolumeSliderSize = {50, 20};
constexpr gfx::Size kMuteButtonSize = {20, 20};
constexpr int kMuteButtonIconSize = 16;

void RecordMetadataHistogram(
    MediaNotificationViewModernImpl::Metadata metadata) {
  UMA_HISTOGRAM_ENUMERATION(
      MediaNotificationViewModernImpl::kMetadataHistogramName, metadata);
}

class MediaButton : public views::ImageButton {
 public:
  METADATA_HEADER(MediaButton);
  MediaButton(PressedCallback callback, int icon_size, gfx::Size button_size)
      : ImageButton(callback), icon_size_(icon_size) {
    SetHasInkDropActionOnClick(true);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  button_size.height() / 2);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
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
    views::InkDrop::Get(this)->SetBaseColor(foreground_color_);

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
  SkColor foreground_color_ = gfx::kPlaceholderColor;
  SkColor foreground_disabled_color_ = gfx::kPlaceholderColor;
  int icon_size_;
};

BEGIN_METADATA(MediaButton, views::ImageButton)
END_METADATA

}  // anonymous namespace

// static
const char MediaNotificationViewModernImpl::kArtworkHistogramName[] =
    "Media.Notification.ArtworkPresent";

// static
const char MediaNotificationViewModernImpl::kMetadataHistogramName[] =
    "Media.Notification.MetadataPresent";

MediaNotificationViewModernImpl::MediaNotificationViewModernImpl(
    MediaNotificationContainer* container,
    base::WeakPtr<MediaNotificationItem> item,
    std::unique_ptr<views::View> notification_controls_view,
    std::unique_ptr<views::View> notification_footer_view,
    int notification_width,
    absl::optional<NotificationTheme> theme)
    : container_(container), item_(std::move(item)), theme_(theme) {
  DCHECK(container_);

  DCHECK(notification_controls_view);

  SetPreferredSize(kMediaNotificationViewBaseSize);

  DCHECK(notification_width >= kMediaNotificationViewBaseSize.width())
      << "MediaNotificationViewModernImpl expects a width of at least "
      << kMediaNotificationViewBaseSize.width();
  auto border_insets = gfx::Insets::VH(
      0, (kMediaNotificationViewBaseSize.width() - notification_width) / 2);
  SetBorder(views::CreateEmptyBorder(border_insets));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  bool is_cros = theme_.has_value();
  if (is_cros) {
    // We don't want the background to paint the artwork since we're painting it
    // ourselves.
    SetBackground(std::make_unique<MediaNotificationBackgroundAshImpl>(
        /*paint_artwork=*/false));
  } else {
    // Force the artwork width to be zero since we're painting the artwork
    // ourselves.
    SetBackground(std::make_unique<MediaNotificationBackgroundImpl>(
        message_center::kNotificationCornerRadius,
        message_center::kNotificationCornerRadius,
        /*artwork_max_width_pct=*/0));
  }

  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);

  {
    // The button container contains media_controls_container_ and
    // notification_controls_view.
    auto buttons_container = std::make_unique<views::View>();
    buttons_container->SetPreferredSize(kButtonsContainerSize);
    auto* buttons_container_layout =
        buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
    buttons_container_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    // The media controls container contains buttons for media playback. This
    // includes play/pause, fast-forward/rewind, and skip controls.
    auto media_controls_container = std::make_unique<views::View>();
    media_controls_container->SetPreferredSize(kMediaControlsContainerSize);
    auto* media_controls_layout = media_controls_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            kMediaControlsContainerInsets, kMediaControlsButtonSpacing));
    media_controls_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    media_controls_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    // Media controls should always be presented left-to-right,
    // regardless of the local UI direction.
    media_controls_container->SetMirrored(false);

    CreateMediaButton(media_controls_container.get(),
                      MediaSessionAction::kPreviousTrack);
    CreateMediaButton(media_controls_container.get(),
                      MediaSessionAction::kSeekBackward);

    {
      auto play_pause_button = std::make_unique<MediaButton>(
          views::Button::PressedCallback(), kPlayPauseIconSize,
          kPlayPauseButtonSize);
      play_pause_button->SetCallback(
          base::BindRepeating(&MediaNotificationViewModernImpl::ButtonPressed,
                              base::Unretained(this), play_pause_button.get()));
      play_pause_button->set_tag(static_cast<int>(MediaSessionAction::kPlay));
      play_pause_button_ =
          media_controls_container->AddChildView(std::move(play_pause_button));
    }

    CreateMediaButton(media_controls_container.get(),
                      MediaSessionAction::kSeekForward);
    CreateMediaButton(media_controls_container.get(),
                      MediaSessionAction::kNextTrack);

    media_controls_container_ =
        buttons_container->AddChildView(std::move(media_controls_container));
    buttons_container_layout->SetFlexForView(media_controls_container_, 1);

    notification_controls_view->SetProperty(views::kMarginsKey,
                                            kNotificationControlsInsets);
    buttons_container->AddChildView(std::move(notification_controls_view));

    AddChildView(std::move(buttons_container));
  }

  auto progress_view = std::make_unique<MediaControlsProgressView>(
      base::BindRepeating(&MediaNotificationViewModernImpl::SeekTo,
                          base::Unretained(this)),
      true /* is_modern_notification */);
  progress_view->SetProperty(views::kMarginsKey, kProgressBarInsets);
  progress_ = AddChildView(std::move(progress_view));
  progress_->SetVisible(true);

  {
    // The info container contains the notification artwork, the labels for the
    // title and artist text, the picture in picture button, and the dismiss
    // button.
    auto info_container = std::make_unique<views::View>();
    info_container->SetPreferredSize(kInfoContainerSize);

    auto* info_container_layout =
        info_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, kInfoContainerInsets,
            kInfoContainerSpacing));
    info_container_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    {
      auto artwork_container = std::make_unique<views::View>();
      artwork_container->SetPreferredSize(kArtworkSize);

      auto* artwork_container_layout = artwork_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
      artwork_container_layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      artwork_container_layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      {
        auto artwork = std::make_unique<MediaArtworkView>(
            kArtworkVignetteCornerRadius, kArtworkSize, kFaviconSize);
        artwork_ = artwork_container->AddChildView(std::move(artwork));
      }

      artwork_container_ =
          info_container->AddChildView(std::move(artwork_container));
    }

    {
      auto labels_container = std::make_unique<views::View>();

      labels_container->SetPreferredSize(
          gfx::Size(kLabelsContainerBaseSize.width() -
                        kNotificationControlsInsets.width(),
                    kLabelsContainerBaseSize.height()));

      auto* labels_container_layout_manager =
          labels_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
      labels_container_layout_manager->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      labels_container_layout_manager->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStart);

      {
        auto title_label = std::make_unique<views::Label>(
            base::EmptyString16(), views::style::CONTEXT_LABEL,
            views::style::STYLE_PRIMARY);
        title_label->SetLineHeight(kTitleArtistLineHeight);
        title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        title_label_ = labels_container->AddChildView(std::move(title_label));
      }

      {
        auto subtitle_label = std::make_unique<views::Label>(
            base::EmptyString16(), views::style::CONTEXT_LABEL,
            views::style::STYLE_SECONDARY);
        subtitle_label->SetLineHeight(18);
        subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        subtitle_label_ =
            labels_container->AddChildView(std::move(subtitle_label));
      }

      info_container->AddChildView(std::move(labels_container));
    }

    AddChildView(std::move(info_container));
  }

  {
    // This view contains pip button, mute button and cast buttons.
    auto util_buttons_container = std::make_unique<views::View>();
    util_buttons_container->SetPreferredSize(kUtilButtonsContainerSize);
    auto* util_buttons_layout = util_buttons_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            kUtilButtonsContainerInsets, kUtilButtonsSpacing));
    util_buttons_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    util_buttons_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    if (item_->GetSourceType() != SourceType::kCast) {
      // The picture-in-picture button appears directly under the media
      // labels.
      auto picture_in_picture_button = std::make_unique<MediaButton>(
          views::Button::PressedCallback(), kPipButtonIconSize, kPipButtonSize);
      picture_in_picture_button->SetCallback(base::BindRepeating(
          &MediaNotificationViewModernImpl::ButtonPressed,
          base::Unretained(this), picture_in_picture_button.get()));
      picture_in_picture_button->set_tag(
          static_cast<int>(MediaSessionAction::kEnterPictureInPicture));
      picture_in_picture_button_ = util_buttons_container->AddChildView(
          std::move(picture_in_picture_button));
    }

    if (notification_footer_view) {
      auto* footer_view = util_buttons_container->AddChildView(
          std::move(notification_footer_view));
      util_buttons_layout->SetFlexForView(footer_view, 1);
    }

    if (item_->GetSourceType() == SourceType::kCast) {
      auto volume_slider = std::make_unique<MediaNotificationVolumeSliderView>(
          base::BindRepeating(&MediaNotificationViewModernImpl::SetVolume,
                              base::Unretained(this)));
      volume_slider->SetPreferredSize(kVolumeSliderSize);
      volume_slider_ =
          util_buttons_container->AddChildView(std::move(volume_slider));
    }

    auto mute_button =
        std::make_unique<views::ToggleImageButton>(base::BindRepeating(
            &MediaNotificationViewModernImpl::OnMuteButtonClicked,
            base::Unretained(this)));
    mute_button->SetPreferredSize(kMuteButtonSize);
    mute_button->SetImageHorizontalAlignment(
        views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
    mute_button->SetImageVerticalAlignment(
        views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
    mute_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_MUTE));
    mute_button->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_MUTE));
    mute_button_ = util_buttons_container->AddChildView(std::move(mute_button));

    AddChildView(std::move(util_buttons_container));
  }

  if (item_)
    item_->SetView(this);
}

MediaNotificationViewModernImpl::~MediaNotificationViewModernImpl() {
  if (item_)
    item_->SetView(nullptr);
}

void MediaNotificationViewModernImpl::UpdateCornerRadius(int top_radius,
                                                         int bottom_radius) {
  if (GetMediaNotificationBackground()->UpdateCornerRadius(top_radius,
                                                           bottom_radius)) {
    SchedulePaint();
  }
}

void MediaNotificationViewModernImpl::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListItem;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));

  if (!GetAccessibleName().empty()) {
    node_data->SetNameChecked(GetAccessibleName());
  }
}

void MediaNotificationViewModernImpl::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;

  MediaSessionAction action =
      playing ? MediaSessionAction::kPause : MediaSessionAction::kPlay;
  play_pause_button_->set_tag(static_cast<int>(action));

  bool in_picture_in_picture =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;

  if (picture_in_picture_button_) {
    action = in_picture_in_picture ? MediaSessionAction::kExitPictureInPicture
                                   : MediaSessionAction::kEnterPictureInPicture;
    picture_in_picture_button_->set_tag(static_cast<int>(action));
  }

  UpdateActionButtonsVisibility();

  container_->OnMediaSessionInfoChanged(session_info);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  title_label_->SetText(metadata.title);
  subtitle_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  subtitle_label_->SetText(metadata.source_title);

  // Stores the text to be read by screen readers describing the notification.
  // Contains the title, artist and album separated by hyphens.
  SetAccessibleName(GetAccessibleNameFromMetadata(metadata));

  // The title label should only be a11y-focusable when there is text to be
  // read.
  if (metadata.title.empty()) {
    title_label_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    title_label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    RecordMetadataHistogram(Metadata::kTitle);
  }

  // The subtitle label should only be a11y-focusable when there is text to be
  // read.
  if (metadata.source_title.empty()) {
    subtitle_label_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    subtitle_label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    RecordMetadataHistogram(Metadata::kSource);
  }

  RecordMetadataHistogram(Metadata::kCount);

  container_->OnMediaSessionMetadataChanged(metadata);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;

  UpdateActionButtonsVisibility();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::UpdateWithMediaPosition(
    const media_session::MediaPosition& position) {
  position_ = position;
  progress_->UpdateProgress(position);
}

void MediaNotificationViewModernImpl::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  GetMediaNotificationBackground()->UpdateArtwork(image);

  UMA_HISTOGRAM_BOOLEAN(kArtworkHistogramName, !image.isNull());
  artwork_->SetImage(image);
  artwork_->SetPreferredSize(kArtworkSize);

  UpdateForegroundColor();

  container_->OnMediaArtworkChanged(image);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::UpdateWithFavicon(
    const gfx::ImageSkia& icon) {
  GetMediaNotificationBackground()->UpdateFavicon(icon);

  artwork_->SetFavicon(icon);
  artwork_->SetPreferredSize(kArtworkSize);
  UpdateForegroundColor();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::OnThemeChanged() {
  MediaNotificationView::OnThemeChanged();
  UpdateForegroundColor();
}

void MediaNotificationViewModernImpl::UpdateWithMuteStatus(bool mute) {
  if (mute_button_) {
    mute_button_->SetToggled(mute);
    const auto mute_button_description_id =
        mute ? IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_UNMUTE
             : IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_MUTE;
    mute_button_->SetTooltipText(
        l10n_util::GetStringUTF16(mute_button_description_id));
    mute_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(mute_button_description_id));
  }

  if (volume_slider_)
    volume_slider_->SetMute(mute);
}

void MediaNotificationViewModernImpl::UpdateWithVolume(float volume) {
  if (volume_slider_)
    volume_slider_->SetVolume(volume);
}

void MediaNotificationViewModernImpl::UpdateDeviceSelectorVisibility(
    bool visible) {
  GetMediaNotificationBackground()->UpdateDeviceSelectorAvailability(visible);
}

void MediaNotificationViewModernImpl::UpdateActionButtonsVisibility() {
  for (auto* view : media_controls_container_->children()) {
    views::Button* action_button = views::Button::AsButton(view);
    bool should_show = base::Contains(enabled_actions_,
                                      GetActionFromButtonTag(*action_button));
    bool should_invalidate = should_show != action_button->GetVisible();

    action_button->SetVisible(should_show);

    if (should_invalidate)
      action_button->InvalidateLayout();
  }

  if (picture_in_picture_button_) {
    const bool should_show_pip =
        base::ranges::any_of(enabled_actions_, [](MediaSessionAction action) {
          return action == MediaSessionAction::kEnterPictureInPicture ||
                 action == MediaSessionAction::kExitPictureInPicture;
        });

    if (picture_in_picture_button_->GetVisible() != should_show_pip) {
      picture_in_picture_button_->SetVisible(should_show_pip);
      picture_in_picture_button_->InvalidateLayout();
    }
  }

  container_->OnVisibleActionsChanged(enabled_actions_);
}

void MediaNotificationViewModernImpl::CreateMediaButton(
    views::View* parent_view,
    MediaSessionAction action) {
  auto button = std::make_unique<MediaButton>(
      views::Button::PressedCallback(), kMediaButtonIconSize, kMediaButtonSize);
  button->SetCallback(
      base::BindRepeating(&MediaNotificationViewModernImpl::ButtonPressed,
                          base::Unretained(this), button.get()));
  button->set_tag(static_cast<int>(action));
  parent_view->AddChildView(std::move(button));
}

MediaNotificationBackground*
MediaNotificationViewModernImpl::GetMediaNotificationBackground() {
  return static_cast<MediaNotificationBackground*>(background());
}

void MediaNotificationViewModernImpl::UpdateForegroundColor() {
  if (!GetWidget())
    return;

  const SkColor background =
      GetMediaNotificationBackground()->GetBackgroundColor(*this);
  const SkColor foreground =
      GetMediaNotificationBackground()->GetForegroundColor(*this);
  const SkColor disabled_icon_color =
      SkColorSetA(foreground, gfx::kDisabledControlAlpha);

  NotificationTheme theme;
  if (theme_.has_value()) {
    theme = *theme_;
  } else {
    theme.primary_text_color = foreground;
    theme.secondary_text_color = foreground;
    theme.enabled_icon_color = foreground;
    theme.disabled_icon_color = disabled_icon_color;
    theme.separator_color = SkColorSetA(foreground, 0x1F);
  }

  artwork_->SetBackgroundColor(theme.disabled_icon_color);
  artwork_->SetVignetteColor(background);

  progress_->SetForegroundColor(theme.primary_text_color);
  progress_->SetBackgroundColor(theme.disabled_icon_color);
  progress_->SetTextColor(theme.primary_text_color);

  if (volume_slider_)
    volume_slider_->UpdateColor(theme.primary_text_color,
                                theme.disabled_icon_color);

  if (mute_button_) {
    views::SetImageFromVectorIconWithColor(
        mute_button_, vector_icons::kVolumeUpIcon, kMuteButtonIconSize,
        theme.enabled_icon_color, theme.disabled_icon_color);
    views::SetToggledImageFromVectorIconWithColor(
        mute_button_, vector_icons::kVolumeOffIcon, kMediaButtonIconSize,
        theme.enabled_icon_color, theme.disabled_icon_color);
  }

  // Update the colors for the labels
  title_label_->SetEnabledColor(theme.primary_text_color);
  subtitle_label_->SetEnabledColor(theme.secondary_text_color);

  title_label_->SetBackgroundColor(background);
  subtitle_label_->SetBackgroundColor(background);

  // Update the colors for the toggle buttons (play/pause and
  // picture-in-picture)
  play_pause_button_->SetButtonColor(theme.enabled_icon_color,
                                     theme.disabled_icon_color);

  if (picture_in_picture_button_)
    picture_in_picture_button_->SetButtonColor(theme.enabled_icon_color,
                                               theme.disabled_icon_color);

  // Update the colors for the media control buttons.
  for (views::View* child : media_controls_container_->children()) {
    // Skip the play pause button since it is a special case.
    if (child == play_pause_button_)
      continue;

    MediaButton* button = static_cast<MediaButton*>(child);

    button->SetButtonColor(theme.enabled_icon_color, theme.disabled_icon_color);
  }

  SchedulePaint();
  container_->OnColorsChanged(theme.enabled_icon_color,
                              theme.disabled_icon_color, background);
}

void MediaNotificationViewModernImpl::ButtonPressed(views::Button* button) {
  if (item_)
    item_->OnMediaSessionActionButtonPressed(GetActionFromButtonTag(*button));
}

void MediaNotificationViewModernImpl::SeekTo(double seek_progress) {
  item_->SeekTo(seek_progress * position_.duration());
}

void MediaNotificationViewModernImpl::OnMuteButtonClicked() {
  item_->SetMute(!mute_button_->GetToggled());
}

void MediaNotificationViewModernImpl::SetVolume(float volume) {
  item_->SetVolume(volume);
  item_->SetMute(volume == 0);
}

views::Button*
MediaNotificationViewModernImpl::picture_in_picture_button_for_testing() const {
  return picture_in_picture_button_;
}

BEGIN_METADATA(MediaNotificationViewModernImpl, views::View)
END_METADATA

}  // namespace media_message_center
