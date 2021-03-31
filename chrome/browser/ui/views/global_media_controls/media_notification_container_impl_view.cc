// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_view_modern_impl.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"

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
constexpr float kDragImageOpacity = 0.7f;
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

// Once the container is dragged this distance, we will not treat the mouse
// press as a click.
constexpr int kMinMovementSquaredToBeDragging = 10;

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
    base::Optional<media_message_center::NotificationTheme> theme)
    : views::Button(base::BindRepeating(
          [](MediaNotificationContainerImplView* view) {
            // If |is_dragging_| is set, this click should be treated as a drag
            // and not fire ContainerClicked().
            if (!view->is_dragging_)
              view->ContainerClicked();
          },
          base::Unretained(this))),
      id_(id),
      foreground_color_(kDefaultForegroundColor),
      background_color_(kDefaultBackgroundColor),
      service_(service),
      is_cros_(theme.has_value()),
      entry_point_(entry_point) {
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
    view =
        std::make_unique<media_message_center::MediaNotificationViewModernImpl>(
            this, std::move(item), std::move(dismiss_button_placeholder),
            kModernUIWidth);
    SetPreferredSize(kModernUISize);
  } else {
    view = std::make_unique<media_message_center::MediaNotificationViewImpl>(
        this, std::move(item), std::move(dismiss_button_placeholder),
        std::u16string(), kWidth, /*should_show_icon=*/false, theme);
    SetPreferredSize(kNormalSize);
  }
  view_ = swipeable_container_->AddChildView(std::move(view));

  // Show a stop cast button for cast notifications.
  if (is_cast_notification &&
      media_router::GlobalMediaControlsCastStartStopEnabled()) {
    AddStopCastButton(cast_item);
  }

  // Show a device selector view for media and supplemental notifications.
  if (!is_cast_notification &&
      (media_router::GlobalMediaControlsCastStartStopEnabled() ||
       base::FeatureList::IsEnabled(
           media::kGlobalMediaControlsSeamlessTransfer))) {
    AddDeviceSelectorView(is_local_media_session);
  }

  ForceExpandedState();

  slide_out_controller_ =
      std::make_unique<views::SlideOutController>(this, this);
}

MediaNotificationContainerImplView::~MediaNotificationContainerImplView() {
  drag_image_widget_.reset();
  for (auto& observer : observers_)
    observer.OnContainerDestroyed(id_);
}

void MediaNotificationContainerImplView::AddedToWidget() {
  if (GetFocusManager())
    GetFocusManager()->AddFocusChangeListener(this);
}

void MediaNotificationContainerImplView::RemovedFromWidget() {
  if (GetFocusManager())
    GetFocusManager()->RemoveFocusChangeListener(this);
}

void MediaNotificationContainerImplView::CreateDragImageWidget() {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_DRAG;
  params.name = "DragImage";
  params.accept_events = false;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.context = GetWidget()->GetNativeWindow();

  drag_image_widget_ = views::UniqueWidgetPtr(
      std::make_unique<views::Widget>(std::move(params)));
  drag_image_widget_->SetOpacity(kDragImageOpacity);

  views::ImageView* image_view =
      drag_image_widget_->SetContentsView(std::make_unique<views::ImageView>());

  SkBitmap bitmap;
  view_->Paint(views::PaintInfo::CreateRootPaintInfo(
      ui::CanvasPainter(&bitmap, GetPreferredSize(), 1.f, SK_ColorTRANSPARENT,
                        true /* is_pixel_canvas */)
          .context(),
      GetPreferredSize()));
  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(bitmap, 1.f);
  image_view->SetImage(image);

  drag_image_widget_->Show();
}

bool MediaNotificationContainerImplView::OnMousePressed(
    const ui::MouseEvent& event) {
  // Reset the |is_dragging_| flag to track whether this is a drag or a click.
  is_dragging_ = false;

  bool button_result = views::Button::OnMousePressed(event);
  if (!ShouldHandleMouseEvent(event, /*is_press=*/true))
    return button_result;

  // Set the |is_mouse_pressed_| flag to mark that we're tracking a potential
  // drag.
  is_mouse_pressed_ = true;

  // Keep track of the initial location to calculate movement.
  initial_drag_location_ = event.location();

  // We want to keep receiving events.
  return true;
}

bool MediaNotificationContainerImplView::OnMouseDragged(
    const ui::MouseEvent& event) {
  bool button_result = views::Button::OnMouseDragged(event);
  if (!ShouldHandleMouseEvent(event, /*is_press=*/false))
    return button_result;

  gfx::Vector2d movement = event.location() - initial_drag_location_;

  // If we ever move enough to be dragging, set the |is_dragging_| flag to
  // prevent this mouse press from firing an |OnContainerClicked| event.
  if (movement.LengthSquared() >= kMinMovementSquaredToBeDragging)
    is_dragging_ = true;

  // If we are in an overlay notification, we want to drag the overlay
  // instead.
  if (dragged_out_) {
    overlay_->SetBoundsConstrained(overlay_->GetWindowBoundsInScreen() +
                                   movement);
    return true;
  }

  if (!drag_image_widget_)
    CreateDragImageWidget();
  drag_image_widget_->SetBounds(GetBoundsInScreen() + movement);

  return true;
}

void MediaNotificationContainerImplView::OnMouseReleased(
    const ui::MouseEvent& event) {
  views::Button::OnMouseReleased(event);
  if (!ShouldHandleMouseEvent(event, /*is_press=*/false))
    return;

  if (dragged_out_)
    return;

  gfx::Vector2d movement = event.location() - initial_drag_location_;

  gfx::Rect bounds_in_screen = GetBoundsInScreen();
  gfx::Rect dragged_bounds = bounds_in_screen + movement;
  swipeable_container_->layer()->SetTransform(gfx::Transform());

  if (!dragged_bounds.Intersects(bounds_in_screen)) {
    for (auto& observer : observers_)
      observer.OnContainerDraggedOut(id_, dragged_bounds);
  }

  drag_image_widget_.reset();
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
  if (overlay_)
    overlay_->UpdateTitle(title_);

  for (auto& observer : observers_)
    observer.OnContainerMetadataChanged();
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
    observer.OnContainerActionsChanged();
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
    if (stop_cast_button_) {
      stop_cast_button_->SetEnabledTextColors(foreground_color_);
      stop_cast_button_->SetInkDropBaseColor(foreground_color_);
    }
  }

  if (background_color_ != background) {
    background_color_ = background;
    UpdateDismissButtonBackground();
    if (stop_button_strip_) {
      stop_button_strip_->SetBackground(
          views::CreateSolidBackground(background_color_));
    }
  }
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
    MediaNotificationContainerObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationContainerImplView::RemoveObserver(
    MediaNotificationContainerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaNotificationContainerImplView::PopOut() {
  // Ensure that we don't keep separator lines around.
  SetBorder(nullptr);

  dragged_out_ = true;
  SetPosition(gfx::Point(0, 0));
}

void MediaNotificationContainerImplView::OnOverlayNotificationShown(
    OverlayMediaNotificationView* overlay) {
  // We can hold |overlay_| indefinitely since |overlay_| owns us.
  DCHECK(!overlay_);
  overlay_ = overlay;
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
  stop_button_strip_->SetBackground(
      views::CreateSolidBackground(background_color_));
  stop_button_strip_->SetPreferredSize(kStopCastButtonStripSize);

  stop_cast_button_ =
      stop_button_strip_->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&MediaNotificationContainerImplView::StopCasting,
                              base::Unretained(this),
                              base::Unretained(cast_item)),
          l10n_util::GetStringUTF16(
              IDS_GLOBAL_MEDIA_CONTROLS_STOP_CASTING_BUTTON_LABEL)));
  stop_cast_button_->SetInkDropMode(InkDropMode::ON);
  stop_cast_button_->SetHasInkDropActionOnClick(true);
  stop_cast_button_->SetInkDropBaseColor(foreground_color_);
  stop_cast_button_->SetInkDropLargeCornerRadius(
      kStopCastButtonStripSize.height());
  stop_cast_button_->SetEnabledTextColors(foreground_color_);
  stop_cast_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  stop_cast_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(1, kStopCastButtonStripSize.height() / 2,
                                     foreground_color_),
      kStopCastButtonBorderInsets));
}

void MediaNotificationContainerImplView::AddDeviceSelectorView(
    bool is_local_media_session) {
  std::unique_ptr<media_router::CastDialogController> cast_controller;
  if (media_router::GlobalMediaControlsCastStartStopEnabled()) {
    cast_controller =
        is_local_media_session
            ? service_->CreateCastDialogControllerForSession(id_)
            : service_->CreateCastDialogControllerForPresentationRequest();
  }
  auto device_selector_view =
      std::make_unique<MediaNotificationDeviceSelectorView>(
          this, std::move(cast_controller),
          /* has_audio_output */ is_local_media_session, audio_sink_id_,
          foreground_color_, background_color_, entry_point_);
  device_selector_view_ = AddChildView(std::move(device_selector_view));
  view_->UpdateCornerRadius(message_center::kNotificationCornerRadius, 0);
}

void MediaNotificationContainerImplView::StopCasting(
    CastMediaNotificationItem* cast_item) {
  media_router::MediaRouterFactory::GetApiForBrowserContext(
      cast_item->profile())
      ->TerminateRoute(cast_item->route_id());

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
    observer.OnContainerDismissed(id_);
}

void MediaNotificationContainerImplView::ForceExpandedState() {
  if (view_) {
    bool should_expand = has_many_actions_ || has_artwork_;
    view_->SetForcedExpandedState(&should_expand);
  }
}

void MediaNotificationContainerImplView::ContainerClicked() {
  for (auto& observer : observers_)
    observer.OnContainerClicked(id_);
}

bool MediaNotificationContainerImplView::ShouldHandleMouseEvent(
    const ui::MouseEvent& event,
    bool is_press) {
  // We only manually handle mouse events for dragging out of the dialog, so if
  // the feature is disabled there's no need to handle the event.
  if (!base::FeatureList::IsEnabled(media::kGlobalMediaControlsOverlayControls))
    return false;

  // We only handle non-press events if we've handled the associated press
  // event.
  if (!is_press && !is_mouse_pressed_)
    return false;

  // We only drag via the left button.
  return event.IsLeftMouseButton();
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

  if (overlay_)
    overlay_->SetSize(new_size);

  SetPreferredSize(new_size);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnContainerSizeChanged();
}

BEGIN_METADATA(MediaNotificationContainerImplView, views::Button)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Title)
END_METADATA
