// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_impl.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "components/media_message_center/media_notification_background_ash_impl.h"
#include "components/media_message_center/media_notification_background_impl.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/notification_theme.h"
#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

// The number of actions supported when the notification is expanded or not.
constexpr size_t kMediaNotificationActionsCount = 3;
constexpr size_t kMediaNotificationExpandedActionsCount = 6;

// Dimensions.
constexpr int kDefaultMarginSize = 8;
constexpr int kMediaButtonIconSize = 16;
constexpr int kTitleArtistLineHeight = 20;
constexpr double kMediaImageMaxWidthPct = 0.3;
constexpr double kMediaImageMaxWidthExpandedPct = 0.4;
constexpr gfx::Size kMediaButtonSize = gfx::Size(36, 36);
constexpr int kMediaButtonRowSeparator = 0;
constexpr auto kMediaTitleArtistInsets = gfx::Insets::TLBR(8, 8, 0, 8);
constexpr auto kIconlessMediaNotificationHeaderInsets =
    gfx::Insets::TLBR(6, 14, 0, 6);
constexpr auto kIconMediaNotificationHeaderInsets =
    gfx::Insets::TLBR(6, 0, 0, 6);
constexpr gfx::Size kMediaNotificationButtonRowSize =
    gfx::Size(124, kMediaButtonSize.height());
constexpr gfx::Size kPipButtonSeparatorViewSize = gfx::Size(20, 24);

// Dimensions for CrOS.
constexpr int kCrOSTitleLineHeight = 20;
constexpr int kCrOSArtistLineHeight = 16;
constexpr int kCrOSMediaButtonRowSeparator = 8;
constexpr int kCrOSHeaderRowSeparator = 16;
constexpr gfx::Size kCrOSMediaButtonSize = gfx::Size(32, 32);
constexpr gfx::Insets kCrOSMediaTitleArtistInsets =
    gfx::Insets::TLBR(0, 8, 12, 0);
constexpr gfx::Size kCrOSMediaNotificationButtonRowSize =
    gfx::Size(124, kCrOSMediaButtonSize.height());
constexpr gfx::Size kCrOSPipButtonSeparatorViewSize = gfx::Size(1, 20);
constexpr auto kCrOSHeaderRowInsets = gfx::Insets::TLBR(16, 16, 0, 16);
constexpr auto kCrOSMainRowInsetsWithArtwork =
    gfx::Insets::TLBR(12, 8, 16, 111);
constexpr auto kCrOSMainRowInsetsWithoutArtwork =
    gfx::Insets::TLBR(12, 8, 16, 16);

size_t GetMaxNumActions(bool expanded) {
  return expanded ? kMediaNotificationExpandedActionsCount
                  : kMediaNotificationActionsCount;
}

void UpdateAppIconVisibility(message_center::NotificationHeaderView* header_row,
                             bool should_show_icon) {
  DCHECK(header_row);

  header_row->SetAppIconVisible(should_show_icon);
  header_row->SetProperty(views::kMarginsKey,
                          should_show_icon
                              ? kIconMediaNotificationHeaderInsets
                              : kIconlessMediaNotificationHeaderInsets);
}

}  // namespace

MediaNotificationViewImpl::MediaNotificationViewImpl(
    MediaNotificationContainer* container,
    base::WeakPtr<MediaNotificationItem> item,
    std::unique_ptr<views::View> header_row_controls_view,
    const std::u16string& default_app_name,
    int notification_width,
    bool should_show_icon,
    std::optional<NotificationTheme> theme)
    : container_(container),
      item_(std::move(item)),
      default_app_name_(default_app_name),
      notification_width_(notification_width),
      theme_(theme),
      is_cros_(theme.has_value()) {
  DCHECK(container_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().SetRoleDescription(l10n_util::GetStringUTF8(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));

  if (is_cros_)
    CreateCrOSHeaderRow(std::move(header_row_controls_view));
  else
    CreateHeaderRow(std::move(header_row_controls_view), should_show_icon);

  // |main_row_| holds the main content of the notification.
  auto main_row = std::make_unique<views::View>();
  main_row_ = AddChildView(std::move(main_row));

  // TODO(crbug.com/40232718): `main_row_` sets the flex property in
  // `UpdateViewForExpandedState`, which means it will always satisfy the
  // constraints passed in during the CalculatePreferredSize phase. So we set it
  // here to not require constraints. If possible, consider removing the
  // following flex property
  main_row_->SetLayoutManagerUseConstrainedSpace(false);

  // |title_artist_row_| contains the title and artist labels.
  auto title_artist_row = std::make_unique<views::View>();
  title_artist_row_layout_ =
      title_artist_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          is_cros_ ? kCrOSMediaTitleArtistInsets : kMediaTitleArtistInsets, 0));
  title_artist_row_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  title_artist_row_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  title_artist_row_ = main_row_->AddChildView(std::move(title_artist_row));

  auto title_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  title_label->SetFontList(base_font_list.Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(is_cros_ ? kCrOSTitleLineHeight
                                      : kTitleArtistLineHeight);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_ = title_artist_row_->AddChildView(std::move(title_label));

  auto artist_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  artist_label->SetLineHeight(is_cros_ ? kCrOSArtistLineHeight
                                       : kTitleArtistLineHeight);
  artist_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  artist_label_ = title_artist_row_->AddChildView(std::move(artist_label));

  // |button_row_| contains the buttons for controlling playback.
  auto button_row = std::make_unique<views::View>();
  auto* button_row_layout =
      button_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          is_cros_ ? kCrOSMediaButtonRowSeparator : kMediaButtonRowSeparator));
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  button_row->SetPreferredSize(is_cros_ ? kCrOSMediaNotificationButtonRowSize
                                        : kMediaNotificationButtonRowSize);
  button_row_ = main_row_->AddChildView(std::move(button_row));

  auto playback_button_container = std::make_unique<views::View>();
  auto* playback_button_container_layout =
      playback_button_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
              is_cros_ ? kCrOSMediaButtonRowSeparator
                       : kMediaButtonRowSeparator));
  playback_button_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  // Media playback controls should always be presented left-to-right,
  // regardless of the local UI direction.
  playback_button_container->SetMirrored(false);
  playback_button_container_ =
      button_row_->AddChildView(std::move(playback_button_container));

  CreateMediaButton(
      MediaSessionAction::kPreviousTrack,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK));
  CreateMediaButton(
      MediaSessionAction::kSeekBackward,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_BACKWARD));

  // |play_pause_button_| toggles playback.
  auto play_pause_button =
      views::CreateVectorToggleImageButton(views::Button::PressedCallback());
  play_pause_button->SetCallback(
      base::BindRepeating(&MediaNotificationViewImpl::ButtonPressed,
                          base::Unretained(this), play_pause_button.get()));
  play_pause_button->set_tag(static_cast<int>(MediaSessionAction::kPlay));
  play_pause_button->SetPreferredSize(is_cros_ ? kCrOSMediaButtonSize
                                               : kMediaButtonSize);
  play_pause_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  play_pause_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY));
  play_pause_button->SetToggledTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE));
  play_pause_button->SetFlipCanvasOnPaintForRTLUI(false);
  play_pause_button_ =
      playback_button_container_->AddChildView(std::move(play_pause_button));

  CreateMediaButton(
      MediaSessionAction::kSeekForward,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_FORWARD));
  CreateMediaButton(
      MediaSessionAction::kNextTrack,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK));

  auto pip_button_separator_view = std::make_unique<views::View>();
  auto* pip_button_separator_view_layout =
      pip_button_separator_view->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
  pip_button_separator_view_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  pip_button_separator_view_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  pip_button_separator_view->SetPreferredSize(
      is_cros_ ? kCrOSPipButtonSeparatorViewSize : kPipButtonSeparatorViewSize);

  auto pip_button_separator_stroke = std::make_unique<views::View>();
  pip_button_separator_stroke->SetPreferredSize(
      gfx::Size(1, is_cros_ ? kCrOSPipButtonSeparatorViewSize.height()
                            : kPipButtonSeparatorViewSize.height()));

  pip_button_separator_view->AddChildView(
      std::move(pip_button_separator_stroke));
  pip_button_separator_view_ =
      button_row_->AddChildView(std::move(pip_button_separator_view));

  auto picture_in_picture_button =
      views::CreateVectorToggleImageButton(views::Button::PressedCallback());
  picture_in_picture_button->SetCallback(base::BindRepeating(
      &MediaNotificationViewImpl::ButtonPressed, base::Unretained(this),
      picture_in_picture_button.get()));
  picture_in_picture_button->set_tag(
      static_cast<int>(MediaSessionAction::kEnterPictureInPicture));
  picture_in_picture_button->SetPreferredSize(is_cros_ ? kCrOSMediaButtonSize
                                                       : kMediaButtonSize);
  picture_in_picture_button->SetFocusBehavior(
      views::View::FocusBehavior::ALWAYS);
  picture_in_picture_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP));
  picture_in_picture_button->SetToggledTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP));
  picture_in_picture_button->SetFlipCanvasOnPaintForRTLUI(false);
  picture_in_picture_button_ =
      button_row_->AddChildView(std::move(picture_in_picture_button));

  // Use ash style background if we do have a theme.
  if (is_cros_) {
    SetBackground(std::make_unique<MediaNotificationBackgroundAshImpl>());

    for (views::View* button : GetButtons())
      views::InstallCircleHighlightPathGenerator(button);
  } else {
    SetBackground(std::make_unique<MediaNotificationBackgroundImpl>(
        message_center::kNotificationCornerRadius,
        message_center::kNotificationCornerRadius, kMediaImageMaxWidthPct));
  }

  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);
  UpdateViewForExpandedState();

  if (header_row_) {
    header_row_->SetExpandButtonEnabled(GetExpandable());
  }

  if (item_) {
    item_->SetView(this);
  }
}

MediaNotificationViewImpl::~MediaNotificationViewImpl() {
  if (item_) {
    item_->SetView(nullptr);
  }
}

void MediaNotificationViewImpl::SetExpanded(bool expanded) {
  if (expanded_ == expanded) {
    return;
  }

  expanded_ = expanded;

  UpdateViewForExpandedState();

  PreferredSizeChanged();
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void MediaNotificationViewImpl::UpdateCornerRadius(int top_radius,
                                                   int bottom_radius) {
  if (GetMediaNotificationBackground()->UpdateCornerRadius(top_radius,
                                                           bottom_radius)) {
    SchedulePaint();
  }
}

void MediaNotificationViewImpl::SetForcedExpandedState(
    bool* forced_expanded_state) {
  if (forced_expanded_state) {
    if (forced_expanded_state_ == *forced_expanded_state) {
      return;
    }
    forced_expanded_state_ = *forced_expanded_state;
  } else {
    if (!forced_expanded_state_.has_value()) {
      return;
    }
    forced_expanded_state_ = std::nullopt;
  }

  if (header_row_) {
    header_row_->SetExpandButtonEnabled(GetExpandable());
  }
  UpdateViewForExpandedState();
}

void MediaNotificationViewImpl::UpdateWithMediaSessionInfo(
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
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void MediaNotificationViewImpl::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  auto& app_name =
      metadata.source_title.empty() ? default_app_name_ : metadata.source_title;

  if (header_row_) {
    header_row_->SetAppNameElideBehavior(gfx::ELIDE_HEAD);
    header_row_->SetAppName(app_name);
    header_row_->SetSummaryText(metadata.album);
  } else {
    cros_header_label_->SetText(app_name);
  }

  title_label_->SetText(metadata.title);
  artist_label_->SetText(metadata.artist);

  GetViewAccessibility().SetName(GetAccessibleNameFromMetadata(metadata));

  // The title label should only be a11y-focusable when there is text to be
  // read.
  if (metadata.title.empty()) {
    title_label_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    title_label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  // The artist label should only be a11y-focusable when there is text to be
  // read.
  if (metadata.artist.empty()) {
    artist_label_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    artist_label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  container_->OnMediaSessionMetadataChanged(metadata);

  MaybeShowOrHideArtistLabel();
  PreferredSizeChanged();
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void MediaNotificationViewImpl::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;

  if (header_row_) {
    header_row_->SetExpandButtonEnabled(GetExpandable());
  }
  UpdateViewForExpandedState();

  PreferredSizeChanged();
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void MediaNotificationViewImpl::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  GetMediaNotificationBackground()->UpdateArtwork(image);

  has_artwork_ = !image.isNull();
  UpdateViewForExpandedState();

  if (GetWidget()) {
    UpdateForegroundColor();
  }

  container_->OnMediaArtworkChanged(image);

  MaybeShowOrHideArtistLabel();
  PreferredSizeChanged();
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void MediaNotificationViewImpl::UpdateWithFavicon(const gfx::ImageSkia& icon) {
  GetMediaNotificationBackground()->UpdateFavicon(icon);

  if (GetWidget()) {
    UpdateForegroundColor();
  }
  SchedulePaint();
}

void MediaNotificationViewImpl::UpdateWithVectorIcon(
    const gfx::VectorIcon* vector_icon) {
  if (!header_row_) {
    return;
  }

  vector_header_icon_ = vector_icon;
  UpdateAppIconVisibility(header_row_, vector_header_icon_ != nullptr);
  if (GetWidget()) {
    UpdateForegroundColor();
  }
}

void MediaNotificationViewImpl::UpdateDeviceSelectorVisibility(bool visible) {
  GetMediaNotificationBackground()->UpdateDeviceSelectorVisibility(visible);
}

void MediaNotificationViewImpl::OnThemeChanged() {
  MediaNotificationView::OnThemeChanged();
  UpdateForegroundColor();
}

views::Button* MediaNotificationViewImpl::GetHeaderRowForTesting() const {
  return header_row_;
}

std::u16string MediaNotificationViewImpl::GetSourceTitleForTesting() const {
  return header_row_ ? header_row_->app_name_for_testing()  // IN-TEST
                     : cros_header_label_->GetText();
}

void MediaNotificationViewImpl::UpdateActionButtonsVisibility() {
  base::flat_set<MediaSessionAction> ignored_actions = {
      GetPlayPauseIgnoredAction(GetActionFromButtonTag(*play_pause_button_)),
      GetPictureInPictureIgnoredAction(
          GetActionFromButtonTag(*picture_in_picture_button_))};

  base::flat_set<MediaSessionAction> visible_actions =
      GetTopVisibleActions(enabled_actions_, ignored_actions,
                           GetMaxNumActions(GetActuallyExpanded()));

  for (views::View* view : GetButtons()) {
    views::Button* action_button = views::Button::AsButton(view);
    bool should_show =
        base::Contains(visible_actions, GetActionFromButtonTag(*action_button));
    bool should_invalidate = should_show != action_button->GetVisible();

    action_button->SetVisible(should_show);

    if (should_invalidate) {
      action_button->InvalidateLayout();
    }

    if (action_button == picture_in_picture_button_) {
      pip_button_separator_view_->SetVisible(should_show);

      if (should_invalidate) {
        pip_button_separator_view_->InvalidateLayout();
      }
    }
  }

  // We want to give the container a list of all possibly visible actions, and
  // not just currently visible actions so it can make a decision on whether or
  // not to force an expanded state.
  container_->OnVisibleActionsChanged(GetTopVisibleActions(
      enabled_actions_, ignored_actions, GetMaxNumActions(true)));
}

void MediaNotificationViewImpl::UpdateViewForExpandedState() {
  bool expanded = GetActuallyExpanded();

  // Adjust the layout of the |main_row_| based on the expanded state. If the
  // notification is expanded then the buttons should be below the title/artist
  // information. If it is collapsed then the buttons will be to the right.
  if (is_cros_) {
    static_cast<views::BoxLayout*>(button_row_->GetLayoutManager())
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical,
            has_artwork_ ? kCrOSMainRowInsetsWithArtwork
                         : kCrOSMainRowInsetsWithoutArtwork,
            0))
        ->SetDefaultFlex(1);
  } else if (expanded) {
    static_cast<views::BoxLayout*>(button_row_->GetLayoutManager())
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical,
            gfx::Insets::TLBR(
                kDefaultMarginSize, kDefaultMarginSize, kDefaultMarginSize,
                has_artwork_
                    ? (notification_width_ * kMediaImageMaxWidthExpandedPct)
                    : kDefaultMarginSize),
            kDefaultMarginSize))
        ->SetDefaultFlex(1);
  } else {
    static_cast<views::BoxLayout*>(button_row_->GetLayoutManager())
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::TLBR(
                0, kDefaultMarginSize, 14,
                has_artwork_ ? (notification_width_ * kMediaImageMaxWidthPct)
                             : kDefaultMarginSize),
            kDefaultMarginSize, true))
        ->SetFlexForView(title_artist_row_, 1);
  }

  main_row_->DeprecatedLayoutImmediately();

  if (GetMediaNotificationBackground()->UpdateArtworkMaxWidthPct(
          expanded ? kMediaImageMaxWidthExpandedPct : kMediaImageMaxWidthPct)) {
    SchedulePaint();
  }

  if (header_row_) {
    header_row_->SetExpanded(expanded);
  }
  container_->OnExpanded(expanded);

  UpdateActionButtonsVisibility();
}

void MediaNotificationViewImpl::CreateMediaButton(
    MediaSessionAction action,
    const std::u16string& accessible_name) {
  auto button =
      views::CreateVectorImageButton(views::Button::PressedCallback());
  button->SetCallback(
      base::BindRepeating(&MediaNotificationViewImpl::ButtonPressed,
                          base::Unretained(this), button.get()));
  button->set_tag(static_cast<int>(action));
  button->SetPreferredSize(is_cros_ ? kCrOSMediaButtonSize : kMediaButtonSize);
  button->GetViewAccessibility().SetName(accessible_name);
  button->SetTooltipText(accessible_name);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetFlipCanvasOnPaintForRTLUI(false);
  playback_button_container_->AddChildView(std::move(button));
}

void MediaNotificationViewImpl::CreateHeaderRow(
    std::unique_ptr<views::View> header_row_controls_view,
    bool should_show_icon) {
  auto header_row = std::make_unique<message_center::NotificationHeaderView>(
      base::BindRepeating(
          [](MediaNotificationViewImpl* view) {
            view->SetExpanded(!view->expanded_);
            view->container_->OnHeaderClicked();
          },
          base::Unretained(this)));

  if (header_row_controls_view) {
    header_row_controls_view_ =
        header_row->AddChildView(std::move(header_row_controls_view));
  }

  header_row->SetAppName(default_app_name_);
  header_row->SetFocusBehavior(FocusBehavior::NEVER);

  if (should_show_icon) {
    header_row->ClearAppIcon();
  }
  UpdateAppIconVisibility(header_row.get(), should_show_icon);

  header_row_ = AddChildView(std::move(header_row));
}

void MediaNotificationViewImpl::CreateCrOSHeaderRow(
    std::unique_ptr<views::View> header_row_controls_view) {
  auto cros_header_row = std::make_unique<views::View>();
  auto* header_row_layout =
      cros_header_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kCrOSHeaderRowInsets,
          kCrOSHeaderRowSeparator));

  auto header_label = std::make_unique<views::Label>(
      default_app_name_, views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  header_label->SetFontList(base_font_list.Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::MEDIUM));
  header_label->SetLineHeight(kCrOSTitleLineHeight);
  header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header_label->SetAutoColorReadabilityEnabled(false);

  cros_header_label_ = cros_header_row->AddChildView(std::move(header_label));
  header_row_layout->SetFlexForView(cros_header_label_, 1);

  if (header_row_controls_view) {
    header_row_controls_view_ =
        cros_header_row->AddChildView(std::move(header_row_controls_view));
  }

  AddChildView(std::move(cros_header_row));
}

MediaNotificationBackground*
MediaNotificationViewImpl::GetMediaNotificationBackground() {
  return static_cast<MediaNotificationBackground*>(background());
}

bool MediaNotificationViewImpl::GetExpandable() const {
  if (forced_expanded_state_.has_value()) {
    return false;
  }

  base::flat_set<MediaSessionAction> ignored_actions = {
      GetPlayPauseIgnoredAction(GetActionFromButtonTag(*play_pause_button_)),
      GetPictureInPictureIgnoredAction(
          GetActionFromButtonTag(*picture_in_picture_button_))};

  // If we can show more notifications if we were expanded then we should be
  // expandable.
  return GetTopVisibleActions(enabled_actions_, ignored_actions,
                              GetMaxNumActions(true))
             .size() > kMediaNotificationActionsCount;
}

bool MediaNotificationViewImpl::GetActuallyExpanded() const {
  if (forced_expanded_state_.has_value()) {
    return forced_expanded_state_.value();
  }
  return expanded_ && GetExpandable();
}

void MediaNotificationViewImpl::UpdateForegroundColor() {
  const SkColor background =
      GetMediaNotificationBackground()->GetBackgroundColor(*this);
  const SkColor foreground =
      GetMediaNotificationBackground()->GetForegroundColor(*this);

  NotificationTheme theme;
  if (theme_.has_value()) {
    theme = *theme_;
  } else {
    theme.primary_text_color = foreground;
    theme.secondary_text_color = foreground;
    theme.enabled_icon_color = foreground;
    theme.disabled_icon_color =
        SkColorSetA(foreground, gfx::kDisabledControlAlpha);
    theme.separator_color = SkColorSetA(foreground, 0x1F);
  }

  title_label_->SetEnabledColor(theme.primary_text_color);
  artist_label_->SetEnabledColor(theme.secondary_text_color);

  if (header_row_) {
    header_row_->SetColor(theme.primary_text_color);
    header_row_->SetBackgroundColor(background);
  } else {
    cros_header_label_->SetEnabledColor(theme.primary_text_color);
  }

  if (vector_header_icon_ && header_row_) {
    header_row_->SetAppIcon(gfx::CreateVectorIcon(
        *vector_header_icon_, message_center::kSmallImageSizeMD,
        theme.enabled_icon_color));
  }

  title_label_->SetBackgroundColor(background);
  artist_label_->SetBackgroundColor(background);

  pip_button_separator_view_->children().front()->SetBackground(
      views::CreateSolidBackground(theme.separator_color));

  // Update play/pause button images.
  views::SetImageFromVectorIconWithColor(
      play_pause_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kPlay),
      kMediaButtonIconSize, theme.enabled_icon_color,
      theme.disabled_icon_color);
  views::SetToggledImageFromVectorIconWithColor(
      play_pause_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kPause),
      kMediaButtonIconSize, theme.enabled_icon_color,
      theme.disabled_icon_color);

  views::SetImageFromVectorIconWithColor(
      picture_in_picture_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kEnterPictureInPicture),
      kMediaButtonIconSize, theme.enabled_icon_color,
      theme.disabled_icon_color);
  views::SetToggledImageFromVectorIconWithColor(
      picture_in_picture_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kExitPictureInPicture),
      kMediaButtonIconSize, theme.enabled_icon_color,
      theme.disabled_icon_color);

  // Update action buttons.
  for (views::View* child : playback_button_container_->children()) {
    // Skip the play pause button since it is a special case.
    if (child == play_pause_button_) {
      continue;
    }

    views::ImageButton* button = static_cast<views::ImageButton*>(child);

    views::SetImageFromVectorIconWithColor(
        button, *GetVectorIconForMediaAction(GetActionFromButtonTag(*button)),
        kMediaButtonIconSize, theme.enabled_icon_color,
        theme.disabled_icon_color);

    button->SchedulePaint();
  }

  container_->OnColorsChanged(theme.enabled_icon_color,
                              theme.disabled_icon_color, background);
}

void MediaNotificationViewImpl::ButtonPressed(views::Button* button) {
  if (item_)
    item_->OnMediaSessionActionButtonPressed(GetActionFromButtonTag(*button));
}

void MediaNotificationViewImpl::MaybeShowOrHideArtistLabel() {
  if (!is_cros_) {
    return;
  }

  artist_label_->SetVisible(!artist_label_->GetText().empty() || has_artwork_);
}

std::vector<raw_ptr<views::View, VectorExperimental>>
MediaNotificationViewImpl::GetButtons() {
  auto buttons = button_row_->children();
  buttons.insert(buttons.cbegin(),
                 playback_button_container_->children().cbegin(),
                 playback_button_container_->children().cend());
  std::erase_if(buttons, [](views::View* view) {
    return !(views::IsViewClass<views::ImageButton>(view) ||
             views::IsViewClass<views::ToggleImageButton>(view));
  });
  return buttons;
}

BEGIN_METADATA(MediaNotificationViewImpl)
ADD_READONLY_PROPERTY_METADATA(bool, Expandable)
ADD_READONLY_PROPERTY_METADATA(bool, ActuallyExpanded)
END_METADATA

}  // namespace media_message_center
