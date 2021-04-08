// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_modern_impl.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "components/media_message_center/media_notification_background_impl.h"
#include "components/media_message_center/media_notification_constants.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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
constexpr gfx::Insets kNotificationControlsInsets = {8, 0, 0, 8};
constexpr gfx::Size kButtonsContainerSize = {
    kMediaNotificationViewBaseSize.width(), 40};
constexpr gfx::Size kMediaControlsContainerSize = {
    328 /* base width - dismissbutton size - margin*/, 32};
constexpr gfx::Insets kMediaControlsContainerInsets = {0, 22, 0, 0};
constexpr int kMediaControlsButtonSpacing = 16;
constexpr gfx::Insets kProgressBarInsets = {0, 16, 0, 16};
constexpr gfx::Insets kInfoContainerInsets = {0, 15, 0, 16};
constexpr int kInfoContainerSpacing = 12;
constexpr gfx::Size kUtilButtonsContainerSize = {
    kMediaNotificationViewBaseSize.width(), 39};
constexpr gfx::Insets kUtilButtonsContainerInsets = {7, 16, 12, 16};
constexpr int kUtilButtonsSpacing = 8;

constexpr int kTitleArtistLineHeight = 20;
constexpr gfx::Size kMediaButtonSize = {24, 24};
constexpr gfx::Size kPlayPauseButtonSize = {32, 32};
constexpr int kMediaButtonIconSize = 18;

// An image view with a rounded rectangle vignette
class MediaArtworkView : public views::ImageView {
 public:
  METADATA_HEADER(MediaArtworkView);
  explicit MediaArtworkView(float corner_radius)
      : corner_radius_(corner_radius) {}

  void SetVignetteColor(const SkColor& vignette_color) {
    if (vignette_color_ == vignette_color)
      return;
    vignette_color_ = vignette_color;
    OnPropertyChanged(&vignette_color_, views::kPropertyEffectsPaint);
  }
  SkColor GetVignetteColor() const { return vignette_color_; }

  // ImageView
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  SkColor vignette_color_ = gfx::kPlaceholderColor;
  float corner_radius_;
};

BEGIN_METADATA(MediaArtworkView, views::ImageView)
ADD_PROPERTY_METADATA(SkColor, VignetteColor)
END_METADATA

void MediaArtworkView::OnPaint(gfx::Canvas* canvas) {
  views::ImageView::OnPaint(canvas);
  auto path = SkPath().addRoundRect(RectToSkRect(GetLocalBounds()),
                                    corner_radius_, corner_radius_);
  path.toggleInverseFillType();
  cc::PaintFlags paint_flags;
  paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(vignette_color_);
  canvas->DrawPath(path, paint_flags);
}

void RecordMetadataHistogram(
    MediaNotificationViewModernImpl::Metadata metadata) {
  UMA_HISTOGRAM_ENUMERATION(
      MediaNotificationViewModernImpl::kMetadataHistogramName, metadata);
}

const gfx::VectorIcon* GetVectorIconForMediaAction(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return &kMediaPreviousTrackIcon;
    case MediaSessionAction::kSeekBackward:
      return &kMediaSeekBackwardIcon;
    case MediaSessionAction::kPlay:
      return &kPlayArrowIcon;
    case MediaSessionAction::kPause:
      return &kPauseIcon;
    case MediaSessionAction::kSeekForward:
      return &kMediaSeekForwardIcon;
    case MediaSessionAction::kNextTrack:
      return &kMediaNextTrackIcon;
    case MediaSessionAction::kEnterPictureInPicture:
      return &kMediaEnterPipIcon;
    case MediaSessionAction::kExitPictureInPicture:
      return &kMediaExitPipIcon;
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kSwitchAudioDevice:
    case MediaSessionAction::kToggleMicrophone:
    case MediaSessionAction::kToggleCamera:
    case MediaSessionAction::kHangUp:
      NOTREACHED();
      break;
  }

  return nullptr;
}

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
    int notification_width)
    : container_(container), item_(std::move(item)) {
  DCHECK(container_);

  SetPreferredSize(kMediaNotificationViewBaseSize);

  DCHECK(notification_width >= kMediaNotificationViewBaseSize.width())
      << "MediaNotificationViewModernImpl expects a width of at least "
      << kMediaNotificationViewBaseSize.width();
  auto border_insets = gfx::Insets(
      0, (kMediaNotificationViewBaseSize.width() - notification_width) / 2);
  SetBorder(views::CreateEmptyBorder(border_insets));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  SetBackground(std::make_unique<MediaNotificationBackgroundImpl>(
      message_center::kNotificationCornerRadius,
      message_center::kNotificationCornerRadius, 0));

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

    CreateMediaButton(
        media_controls_container.get(), MediaSessionAction::kPreviousTrack,
        l10n_util::GetStringUTF16(
            IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK));
    CreateMediaButton(
        media_controls_container.get(), MediaSessionAction::kSeekBackward,
        l10n_util::GetStringUTF16(
            IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_BACKWARD));

    {
      auto play_pause_button = views::CreateVectorToggleImageButton(
          views::Button::PressedCallback());
      play_pause_button->SetCallback(
          base::BindRepeating(&MediaNotificationViewModernImpl::ButtonPressed,
                              base::Unretained(this), play_pause_button.get()));
      play_pause_button->set_tag(static_cast<int>(MediaSessionAction::kPlay));
      play_pause_button->SetPreferredSize(kPlayPauseButtonSize);
      play_pause_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      play_pause_button->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY));
      play_pause_button->SetToggledTooltipText(l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE));
      play_pause_button->SetFlipCanvasOnPaintForRTLUI(false);
      play_pause_button_ =
          media_controls_container->AddChildView(std::move(play_pause_button));
    }

    CreateMediaButton(
        media_controls_container.get(), MediaSessionAction::kSeekForward,
        l10n_util::GetStringUTF16(
            IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_FORWARD));
    CreateMediaButton(
        media_controls_container.get(), MediaSessionAction::kNextTrack,
        l10n_util::GetStringUTF16(
            IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK));

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

      // The artwork container will become visible once artwork has been set in
      // UpdateWithMediaArtwork
      artwork_container->SetVisible(false);

      auto* artwork_container_layout = artwork_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
      artwork_container_layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      artwork_container_layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      {
        auto artwork =
            std::make_unique<MediaArtworkView>(kArtworkVignetteCornerRadius);
        artwork_ = artwork_container->AddChildView(std::move(artwork));
      }

      artwork_container_ =
          info_container->AddChildView(std::move(artwork_container));
    }

    {
      auto labels_container = std::make_unique<views::View>();

      labels_container->SetPreferredSize(
          {kLabelsContainerBaseSize.width() -
               kNotificationControlsInsets.width(),
           kLabelsContainerBaseSize.height()});

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
        views::BoxLayout::CrossAxisAlignment::kCenter);

    {
      // The picture-in-picture button appears directly under the media
      // labels.
      auto picture_in_picture_button = views::CreateVectorToggleImageButton(
          views::Button::PressedCallback());
      picture_in_picture_button->SetCallback(base::BindRepeating(
          &MediaNotificationViewModernImpl::ButtonPressed,
          base::Unretained(this), picture_in_picture_button.get()));
      picture_in_picture_button->set_tag(
          static_cast<int>(MediaSessionAction::kEnterPictureInPicture));
      picture_in_picture_button->SetPreferredSize(kPipButtonSize);
      picture_in_picture_button->SetFocusBehavior(
          views::View::FocusBehavior::ALWAYS);
      picture_in_picture_button->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP));
      picture_in_picture_button->SetToggledTooltipText(
          l10n_util::GetStringUTF16(
              IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP));
      picture_in_picture_button->SetFlipCanvasOnPaintForRTLUI(false);
      views::SetImageFromVectorIconWithColor(
          picture_in_picture_button.get(),
          *GetVectorIconForMediaAction(
              MediaSessionAction::kEnterPictureInPicture),
          kPipButtonIconSize, SK_ColorBLACK);
      picture_in_picture_button_ = util_buttons_container->AddChildView(
          std::move(picture_in_picture_button));
    }

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

  if (!accessible_name_.empty())
    node_data->SetName(accessible_name_);
}

void MediaNotificationViewModernImpl::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  play_pause_button_->SetToggled(playing);

  MediaSessionAction action =
      playing ? MediaSessionAction::kPause : MediaSessionAction::kPlay;
  play_pause_button_->set_tag(static_cast<int>(action));

  bool in_picture_in_picture =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  picture_in_picture_button_->SetToggled(in_picture_in_picture);

  action = in_picture_in_picture ? MediaSessionAction::kExitPictureInPicture
                                 : MediaSessionAction::kEnterPictureInPicture;
  picture_in_picture_button_->set_tag(static_cast<int>(action));

  UpdateActionButtonsVisibility();

  container_->OnMediaSessionInfoChanged(session_info);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  title_label_->SetText(metadata.title);
  subtitle_label_->SetText(metadata.source_title);

  accessible_name_ = GetAccessibleNameFromMetadata(metadata);

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

  if (!image.isNull())
    artwork_container_->SetVisible(true);

  artwork_->SetImage(image);
  artwork_->SetPreferredSize(kArtworkSize);
  artwork_->SetVignetteColor(
      GetMediaNotificationBackground()->GetBackgroundColor(*this));

  UpdateForegroundColor();

  container_->OnMediaArtworkChanged(image);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::UpdateWithFavicon(
    const gfx::ImageSkia& icon) {
  GetMediaNotificationBackground()->UpdateFavicon(icon);

  UpdateForegroundColor();
  SchedulePaint();
}

void MediaNotificationViewModernImpl::OnThemeChanged() {
  MediaNotificationView::OnThemeChanged();
  UpdateForegroundColor();
}

void MediaNotificationViewModernImpl::UpdateDeviceSelectorAvailability(
    bool availability) {
  GetMediaNotificationBackground()->UpdateDeviceSelectorAvailability(
      availability);
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

  container_->OnVisibleActionsChanged(enabled_actions_);
}

void MediaNotificationViewModernImpl::CreateMediaButton(
    views::View* parent_view,
    MediaSessionAction action,
    const std::u16string& accessible_name) {
  auto button =
      views::CreateVectorImageButton(views::Button::PressedCallback());
  button->SetCallback(
      base::BindRepeating(&MediaNotificationViewModernImpl::ButtonPressed,
                          base::Unretained(this), button.get()));
  button->set_tag(static_cast<int>(action));
  button->SetPreferredSize(kMediaButtonSize);
  button->SetAccessibleName(accessible_name);
  button->SetTooltipText(accessible_name);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetFlipCanvasOnPaintForRTLUI(false);
  parent_view->AddChildView(std::move(button));
}

MediaNotificationBackground*
MediaNotificationViewModernImpl::GetMediaNotificationBackground() {
  return static_cast<MediaNotificationBackground*>(background());
}

void MediaNotificationViewModernImpl::UpdateForegroundColor() {
  const SkColor background =
      GetMediaNotificationBackground()->GetBackgroundColor(*this);
  const SkColor foreground =
      GetMediaNotificationBackground()->GetForegroundColor(*this);
  const SkColor disabled_icon_color =
      SkColorSetA(foreground, gfx::kDisabledControlAlpha);

  progress_->SetForegroundColor(foreground);
  progress_->SetBackgroundColor(disabled_icon_color);
  progress_->SetTextColor(foreground);

  // Update the colors for the labels
  title_label_->SetEnabledColor(foreground);
  subtitle_label_->SetEnabledColor(disabled_icon_color);

  title_label_->SetBackgroundColor(background);
  subtitle_label_->SetBackgroundColor(background);

  // Update the colors for the toggle buttons (play/pause and
  // picture-in-picture)
  views::SetImageFromVectorIconWithColor(
      play_pause_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kPlay),
      kMediaButtonIconSize, foreground);
  views::SetToggledImageFromVectorIconWithColor(
      play_pause_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kPause),
      kMediaButtonIconSize, foreground, disabled_icon_color);

  views::SetImageFromVectorIconWithColor(
      picture_in_picture_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kEnterPictureInPicture),
      kPipButtonIconSize, foreground);
  views::SetToggledImageFromVectorIconWithColor(
      picture_in_picture_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kExitPictureInPicture),
      kPipButtonIconSize, foreground, disabled_icon_color);

  // Update the colors for the media control buttons.
  for (views::View* child : media_controls_container_->children()) {
    // Skip the play pause button since it is a special case.
    if (child == play_pause_button_)
      continue;

    views::ImageButton* button = static_cast<views::ImageButton*>(child);

    views::SetImageFromVectorIconWithColor(
        button, *GetVectorIconForMediaAction(GetActionFromButtonTag(*button)),
        kMediaButtonIconSize, foreground);

    button->SchedulePaint();
  }

  SchedulePaint();
  container_->OnColorsChanged(foreground, background);
}

void MediaNotificationViewModernImpl::ButtonPressed(views::Button* button) {
  if (item_)
    item_->OnMediaSessionActionButtonPressed(GetActionFromButtonTag(*button));
}

void MediaNotificationViewModernImpl::SeekTo(double seek_progress) {
  item_->SeekTo(seek_progress * position_.duration());
}

BEGIN_METADATA(MediaNotificationViewModernImpl, views::View)
END_METADATA

}  // namespace media_message_center
