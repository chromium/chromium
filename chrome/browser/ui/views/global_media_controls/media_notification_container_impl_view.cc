// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_footer_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_view_modern_impl.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/layer.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(steimel): We need to decide on the correct values here.
constexpr int kWidth = 400;
constexpr int kModernUIWidth = 350;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);
constexpr gfx::Size kModernUISize = gfx::Size(kModernUIWidth, 168);
constexpr gfx::Size kDismissButtonSize = gfx::Size(30, 30);
constexpr int kDismissButtonIconSize = 20;
constexpr int kDismissButtonBackgroundRadius = 15;
constexpr SkColor kDefaultForegroundColor = SK_ColorBLACK;
constexpr SkColor kDefaultBackgroundColor = SK_ColorTRANSPARENT;
constexpr gfx::Insets kStopCastButtonStripInsets{6, 15};
constexpr gfx::Size kStopCastButtonStripSize{400, 30};
constexpr gfx::Insets kStopCastButtonBorderInsets{4, 8};
constexpr gfx::Size kCrOSDismissButtonSize = gfx::Size(20, 20);
constexpr int kCrOSDismissButtonIconSize = 12;
constexpr gfx::Size kModernDismissButtonSize = gfx::Size(14, 14);
constexpr int kModernDismissButtonIconSize = 10;

// The minimum number of enabled and visible user actions such that we should
// force the MediaNotificationView to be expanded.
constexpr int kMinVisibleActionsForExpanding = 4;

}  // anonymous namespace

class MediaNotificationContainerImplView::DismissButton
    : public views::ImageButton {
 public:
  METADATA_HEADER(DismissButton);

  explicit DismissButton(PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    views::ConfigureVectorImageButton(this);
    views::InstallFixedSizeCircleHighlightPathGenerator(
        this, kDismissButtonBackgroundRadius);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  }
  DismissButton(const DismissButton&) = delete;
  DismissButton& operator=(const DismissButton&) = delete;
  ~DismissButton() override = default;
};

BEGIN_METADATA(MediaNotificationContainerImplView,
               DismissButton,
               views::ImageButton)
END_METADATA

MediaNotificationContainerImplView::MediaNotificationContainerImplView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    MediaNotificationService* service,
    GlobalMediaControlsEntryPoint entry_point,
    Profile* profile,
    absl::optional<media_message_center::NotificationTheme> theme)
    : views::Button(base::BindRepeating(
          &MediaNotificationContainerImplView::ContainerClicked,
          base::Unretained(this))),
      id_(id),
      foreground_color_(kDefaultForegroundColor),
      background_color_(kDefaultBackgroundColor),
      service_(service),
      is_cros_(theme.has_value()),
      entry_point_(entry_point),
      profile_(profile) {
  DCHECK(item);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetPreferredSize(kNormalSize);
  SetNotifyEnterExitOnChild(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_BACK_TO_TAB));

  auto swipeable_container = std::make_unique<views::View>();
  swipeable_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  swipeable_container->SetPaintToLayer();
  swipeable_container->layer()->SetFillsBoundsOpaquely(false);
  swipeable_container_ = AddChildView(std::move(swipeable_container));

  gfx::Size dismiss_button_size =
      is_cros_ ? kCrOSDismissButtonSize : kDismissButtonSize;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI))
    dismiss_button_size = kModernDismissButtonSize;

  auto dismiss_button_placeholder = std::make_unique<views::View>();
  dismiss_button_placeholder->SetPreferredSize(dismiss_button_size);
  dismiss_button_placeholder->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  dismiss_button_placeholder_ = dismiss_button_placeholder.get();

  auto dismiss_button_container = std::make_unique<views::View>();
  dismiss_button_container->SetPreferredSize(dismiss_button_size);
  dismiss_button_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  dismiss_button_container->SetVisible(false);
  dismiss_button_container_ = dismiss_button_placeholder_->AddChildView(
      std::move(dismiss_button_container));

  auto dismiss_button = std::make_unique<DismissButton>(base::BindRepeating(
      &MediaNotificationContainerImplView::DismissNotification,
      base::Unretained(this)));
  dismiss_button->SetPreferredSize(dismiss_button_size);
  dismiss_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_DISMISS_ICON_TOOLTIP_TEXT));
  dismiss_button_ =
      dismiss_button_container_->AddChildView(std::move(dismiss_button));
  UpdateDismissButtonIcon();

  // Compute a few things related to |item| before the construction of |view|
  // below moves it.
  const bool is_cast_notification =
      item->SourceType() == media_message_center::SourceType::kCast;
  auto* const cast_item =
      is_cast_notification ? static_cast<CastMediaNotificationItem*>(item.get())
                           : nullptr;
  const bool is_local_media_session =
      item->SourceType() ==
      media_message_center::SourceType::kLocalMediaSession;

  std::unique_ptr<media_message_center::MediaNotificationView> view;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI)) {
    auto footer_view = std::make_unique<MediaNotificationFooterView>(
        is_cast_notification,
        is_cast_notification
            ? base::BindRepeating(
                  &MediaNotificationContainerImplView::StopCasting,
                  base::Unretained(this), base::Unretained(cast_item))
            : views::Button::PressedCallback());
    footer_view_ = footer_view.get();

    view =
        std::make_unique<media_message_center::MediaNotificationViewModernImpl>(
            this, std::move(item), std::move(dismiss_button_placeholder),
            std::move(footer_view), kModernUIWidth);
    SetPreferredSize(kModernUISize);
  } else {
    view = std::make_unique<media_message_center::MediaNotificationViewImpl>(
        this, std::move(item), std::move(dismiss_button_placeholder),
        std::u16string(), kWidth, /*should_show_icon=*/false, theme);
    SetPreferredSize(kNormalSize);
  }
  view_ = swipeable_container_->AddChildView(std::move(view));
  bool gmc_cast_start_stop_enabled =
      media_router::GlobalMediaControlsCastStartStopEnabled() &&
      media_router::MediaRouterEnabled(profile_);
  // Show a stop cast button for cast notifications.
  if (is_cast_notification && gmc_cast_start_stop_enabled &&
      !base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI)) {
    AddStopCastButton(cast_item);
  }

  // Show a device selector view for media and supplemental notifications.
  if (!is_cast_notification &&
      (gmc_cast_start_stop_enabled ||
       base::FeatureList::IsEnabled(
           media::kGlobalMediaControlsSeamlessTransfer))) {
    AddDeviceSelectorView(
        is_local_media_session,
        /* show_expand_button */ !base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsModernUI));
    if (device_selector_view_ && footer_view_) {
      footer_view_->SetDelegate(device_selector_view_);
      device_selector_view_->AddObserver(footer_view_);
    }
  }

  ForceExpandedState();

  slide_out_controller_ =
      std::make_unique<views::SlideOutController>(this, this);
}

MediaNotificationContainerImplView::~MediaNotificationContainerImplView() {
  for (auto& observer : observers_)
    observer.OnMediaItemUIDestroyed(id_);
}

void MediaNotificationContainerImplView::AddedToWidget() {
  if (GetFocusManager())
    GetFocusManager()->AddFocusChangeListener(this);
}

void MediaNotificationContainerImplView::RemovedFromWidget() {
  if (GetFocusManager())
    GetFocusManager()->RemoveFocusChangeListener(this);
}

void MediaNotificationContainerImplView::OnMouseEntered(
    const ui::MouseEvent& event) {
  UpdateDismissButtonVisibility();
}

void MediaNotificationContainerImplView::OnMouseExited(
    const ui::MouseEvent& event) {
  UpdateDismissButtonVisibility();
}

void MediaNotificationContainerImplView::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  UpdateDismissButtonVisibility();
}

void MediaNotificationContainerImplView::OnExpanded(bool expanded) {
  is_expanded_ = expanded;
  OnSizeChanged();
}

void MediaNotificationContainerImplView::OnMediaSessionInfoChanged(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  is_playing_ =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  if (session_info) {
    audio_sink_id_ = session_info->audio_sink_id.value_or(
        media::AudioDeviceDescription::kDefaultDeviceId);
    if (device_selector_view_ &&
        base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsSeamlessTransfer)) {
      device_selector_view_->UpdateCurrentAudioDevice(audio_sink_id_);
    }
  }
}

void MediaNotificationContainerImplView::OnMediaSessionMetadataChanged(
    const media_session::MediaMetadata& metadata) {
  title_ = metadata.title;

  for (auto& observer : observers_)
    observer.OnMediaItemUIMetadataChanged();
}

void MediaNotificationContainerImplView::OnVisibleActionsChanged(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  has_many_actions_ =
      (actions.size() >= kMinVisibleActionsForExpanding ||
       base::Contains(
           actions,
           media_session::mojom::MediaSessionAction::kEnterPictureInPicture) ||
       base::Contains(
           actions,
           media_session::mojom::MediaSessionAction::kExitPictureInPicture));
  ForceExpandedState();

  for (auto& observer : observers_)
    observer.OnMediaItemUIActionsChanged();
}

void MediaNotificationContainerImplView::OnMediaArtworkChanged(
    const gfx::ImageSkia& image) {
  has_artwork_ = !image.isNull();

  UpdateDismissButtonBackground();
  ForceExpandedState();
}

void MediaNotificationContainerImplView::OnColorsChanged(SkColor foreground,
                                                         SkColor background) {
  if (foreground_color_ != foreground) {
    foreground_color_ = foreground;
    UpdateDismissButtonIcon();
    UpdateStopCastButtonIcon();
  }

  if (background_color_ != background) {
    background_color_ = background;
    UpdateDismissButtonBackground();
    UpdateStopCastButtonBackground();
  }
  if (footer_view_)
    footer_view_->OnColorChanged(foreground);

  if (device_selector_view_)
    device_selector_view_->OnColorsChanged(foreground, background);
}

void MediaNotificationContainerImplView::OnHeaderClicked() {
  // Since we disable the expand button, nothing happens on the
  // MediaNotificationView when the header is clicked. Treat the click as if we
  // were clicked directly.
  ContainerClicked();
}

void MediaNotificationContainerImplView::OnAudioSinkChosen(
    const std::string& sink_id) {
  for (auto& observer : observers_) {
    observer.OnAudioSinkChosen(id_, sink_id);
  }
}

void MediaNotificationContainerImplView::OnDeviceSelectorViewSizeChanged() {
  OnSizeChanged();
}

base::CallbackListSubscription MediaNotificationContainerImplView::
    RegisterAudioOutputDeviceDescriptionsCallback(
        MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
            CallbackType callback) {
  return service_->RegisterAudioOutputDeviceDescriptionsCallback(
      std::move(callback));
}

base::CallbackListSubscription MediaNotificationContainerImplView::
    RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  return service_->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      id_, std::move(callback));
}

ui::Layer* MediaNotificationContainerImplView::GetSlideOutLayer() {
  return swipeable_container_->layer();
}

void MediaNotificationContainerImplView::OnSlideOut() {
  DismissNotification();
}

void MediaNotificationContainerImplView::AddObserver(
    global_media_controls::MediaItemUIObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationContainerImplView::RemoveObserver(
    global_media_controls::MediaItemUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::u16string& MediaNotificationContainerImplView::GetTitle() const {
  return title_;
}

views::ImageButton*
MediaNotificationContainerImplView::GetDismissButtonForTesting() {
  return dismiss_button_;
}

views::Button*
MediaNotificationContainerImplView::GetStopCastingButtonForTesting() {
  return stop_cast_button_;
}

void MediaNotificationContainerImplView::AddStopCastButton(
    CastMediaNotificationItem* cast_item) {
  DCHECK(cast_item);
  stop_button_strip_ = AddChildView(std::make_unique<views::View>());
  auto* stop_cast_button_strip_layout =
      stop_button_strip_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kStopCastButtonStripInsets));
  stop_cast_button_strip_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  stop_cast_button_strip_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  stop_button_strip_->SetPreferredSize(kStopCastButtonStripSize);
  UpdateStopCastButtonBackground();

  stop_cast_button_ =
      stop_button_strip_->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&MediaNotificationContainerImplView::StopCasting,
                              base::Unretained(this),
                              base::Unretained(cast_item)),
          l10n_util::GetStringUTF16(
              IDS_GLOBAL_MEDIA_CONTROLS_STOP_CASTING_BUTTON_LABEL)));
  views::InstallRoundRectHighlightPathGenerator(
      stop_cast_button_, gfx::Insets(), kStopCastButtonStripSize.height() / 2);

  views::InkDrop::Get(stop_cast_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  stop_cast_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  stop_cast_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(1, kStopCastButtonStripSize.height() / 2,
                                     foreground_color_),
      kStopCastButtonBorderInsets));
  UpdateStopCastButtonIcon();
}

void MediaNotificationContainerImplView::AddDeviceSelectorView(
    bool is_local_media_session,
    bool show_expand_button) {
  std::unique_ptr<media_router::CastDialogController> cast_controller;
  if (media_router::GlobalMediaControlsCastStartStopEnabled() &&
      media_router::MediaRouterEnabled(profile_)) {
    cast_controller =
        is_local_media_session
            ? service_->CreateCastDialogControllerForSession(id_)
            : service_->CreateCastDialogControllerForPresentationRequest();
  }
  auto device_selector_view =
      std::make_unique<MediaNotificationDeviceSelectorView>(
          this, std::move(cast_controller),
          /* has_audio_output */ is_local_media_session, audio_sink_id_,
          foreground_color_, background_color_, entry_point_,
          show_expand_button);
  device_selector_view_ = AddChildView(std::move(device_selector_view));
  view_->UpdateCornerRadius(message_center::kNotificationCornerRadius, 0);
}

void MediaNotificationContainerImplView::StopCasting(
    CastMediaNotificationItem* cast_item) {
  stop_cast_button_->SetEnabled(false);

  media_router::MediaRouterFactory::GetApiForBrowserContext(
      cast_item->profile())
      ->TerminateRoute(cast_item->route_id());

  // |service_| is nullptr in MediaNotificationContainerImplViewTest.
  if (service_)
    service_->media_item_manager()->FocusDialog();

  feature_engagement::TrackerFactory::GetForBrowserContext(profile_)
      ->NotifyEvent("media_route_stopped_from_gmc");

  GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point_) {
    case GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaToolbarIcon;
      break;
    case GlobalMediaControlsEntryPoint::kPresentation:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaPresentation;
      break;
    case GlobalMediaControlsEntryPoint::kSystemTray:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(
      media_message_center::MediaNotificationItem::kCastStartStopHistogramName,
      action);
}

void MediaNotificationContainerImplView::UpdateStopCastButtonIcon() {
  if (!stop_cast_button_)
    return;
  stop_cast_button_->SetEnabledTextColors(foreground_color_);
  views::InkDrop::Get(stop_cast_button_)->SetBaseColor(foreground_color_);
  stop_cast_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(1, kStopCastButtonStripSize.height() / 2,
                                     foreground_color_),
      kStopCastButtonBorderInsets));
}

void MediaNotificationContainerImplView::UpdateStopCastButtonBackground() {
  if (!stop_button_strip_)
    return;
  stop_button_strip_->SetBackground(
      views::CreateSolidBackground(background_color_));
}

void MediaNotificationContainerImplView::UpdateDismissButtonIcon() {
  int icon_size =
      is_cros_ ? kCrOSDismissButtonIconSize : kDismissButtonIconSize;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI))
    icon_size = kModernDismissButtonIconSize;

  views::SetImageFromVectorIconWithColor(dismiss_button_,
                                         vector_icons::kCloseRoundedIcon,
                                         icon_size, foreground_color_);
}

void MediaNotificationContainerImplView::UpdateDismissButtonBackground() {
  if (!has_artwork_) {
    dismiss_button_container_->SetBackground(nullptr);
    return;
  }

  dismiss_button_container_->SetBackground(views::CreateRoundedRectBackground(
      background_color_, kDismissButtonBackgroundRadius));
}

void MediaNotificationContainerImplView::UpdateDismissButtonVisibility() {
  bool has_focus = false;
  if (GetFocusManager()) {
    views::View* focused_view = GetFocusManager()->GetFocusedView();
    if (focused_view)
      has_focus = Contains(focused_view);
  }

  dismiss_button_container_->SetVisible(IsMouseHovered() || has_focus);
}

void MediaNotificationContainerImplView::DismissNotification() {
  for (auto& observer : observers_)
    observer.OnMediaItemUIDismissed(id_);
}

void MediaNotificationContainerImplView::ForceExpandedState() {
  if (view_) {
    bool should_expand = has_many_actions_ || has_artwork_;
    view_->SetForcedExpandedState(&should_expand);
  }
}

void MediaNotificationContainerImplView::ContainerClicked() {
  for (auto& observer : observers_)
    observer.OnMediaItemUIClicked(id_);
}

void MediaNotificationContainerImplView::OnSizeChanged() {
  gfx::Size new_size;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI)) {
    new_size = kModernUISize;
  } else {
    new_size = is_expanded_ ? kExpandedSize : kNormalSize;
  }

  // |new_size| does not contain the height for the device selector view.
  // If this view is present, we should query it for its preferred height and
  // include that in |new_size|.
  if (device_selector_view_) {
    auto device_selector_view_size = device_selector_view_->GetPreferredSize();
    DCHECK(device_selector_view_size.width() == kWidth);
    new_size.set_height(new_size.height() + device_selector_view_size.height());
    view_->UpdateDeviceSelectorAvailability(
        device_selector_view_->GetVisible());
  }

  SetPreferredSize(new_size);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnMediaItemUISizeChanged();
}

BEGIN_METADATA(MediaNotificationContainerImplView, views::Button)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Title)
END_METADATA
