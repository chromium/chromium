// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/shell.h"  // mash-ok
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#endif

namespace {

const int kMinimumWidth = 460;
const int kMaximumWidth = 1000;
const int kHorizontalMargin = 10;
const float kWindowAlphaValue = 0.85f;
const int kPaddingVertical = 5;
const int kPaddingHorizontal = 10;

namespace {

// A ClientView that overrides NonClientHitTest() so that the whole window area
// acts as a window caption, except a rect specified using set_client_rect().
// ScreenCaptureNotificationUIViews uses this class to make the notification bar
// draggable.
class NotificationBarClientView : public views::ClientView {
 public:
  NotificationBarClientView(views::Widget* widget, views::View* view)
      : views::ClientView(widget, view) {
  }
  ~NotificationBarClientView() override {}

  void set_client_rect(const gfx::Rect& rect) { rect_ = rect; }

  // views::ClientView overrides.
  int NonClientHitTest(const gfx::Point& point) override {
    if (!bounds().Contains(point))
      return HTNOWHERE;
    // The whole window is HTCAPTION, except the |rect_|.
    if (rect_.Contains(gfx::PointAtOffsetFromOrigin(point - origin())))
      return HTCLIENT;

    return HTCAPTION;
  }

 private:
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBarClientView);
};

}  // namespace

// ScreenCaptureNotificationUI implementation using Views.
class ScreenCaptureNotificationUIViews
    : public ScreenCaptureNotificationUI,
      public views::WidgetDelegateView,
      public views::ButtonListener,
      public views::LinkListener {
 public:
  explicit ScreenCaptureNotificationUIViews(const base::string16& text);
  ~ScreenCaptureNotificationUIViews() override;

  // ScreenCaptureNotificationUI interface.
  gfx::NativeViewId OnStarted(const base::Closure& stop_callback) override;

  // views::View overrides.
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  // views::WidgetDelegateView overrides.
  void DeleteDelegate() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool CanActivate() const override;

  // views::ButtonListener interface.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener interface.
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  // Helper to call |stop_callback_|.
  void NotifyStopped();

  const base::string16 text_;
  base::Closure stop_callback_;
  NotificationBarClientView* client_view_;
  views::ImageView* gripper_;
  views::Label* label_;
  views::Button* stop_button_;
  views::Link* hide_link_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIViews);
};

ScreenCaptureNotificationUIViews::ScreenCaptureNotificationUIViews(
    const base::string16& text)
    : text_(text),
      client_view_(nullptr),
      gripper_(nullptr),
      label_(nullptr),
      stop_button_(nullptr),
      hide_link_(nullptr) {
  set_owned_by_client();

  gripper_ = new views::ImageView();
  gripper_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_SCREEN_CAPTURE_NOTIFICATION_GRIP));
  AddChildView(gripper_);

  label_ = new views::Label();
  AddChildView(label_);

  base::string16 stop_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  stop_button_ =
      views::MdTextButton::CreateSecondaryUiBlueButton(this, stop_text);
  AddChildView(stop_button_);

  // TODO(jiayl): IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON is used for the need to
  // merge to M34. Change it to a new IDS_ after the merge.
  hide_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON));
  hide_link_->set_listener(this);
  hide_link_->SetUnderline(false);
  AddChildView(hide_link_);
}

ScreenCaptureNotificationUIViews::~ScreenCaptureNotificationUIViews() {
  stop_callback_.Reset();
  delete GetWidget();
}

gfx::NativeViewId ScreenCaptureNotificationUIViews::OnStarted(
    const base::Closure& stop_callback) {
  stop_callback_ = stop_callback;

  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetText(text_);

  views::Widget* widget = new views::Widget;

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.remove_standard_frame = true;
  params.keep_on_top = true;

#if defined(OS_CHROMEOS)
  // TODO(sergeyu): The notification bar must be shown on the monitor that's
  // being captured. Make sure it's always the case. Currently we always capture
  // the primary monitor.
  if (!features::IsUsingWindowService()) {
    params.context = ash::Shell::GetPrimaryRootWindow();
  } else {
    const display::Display primary_display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    params.mus_properties[ws::mojom::WindowManager::kDisplayId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(primary_display.id());
  }
#endif

  widget->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  widget->Init(params);
  widget->SetAlwaysOnTop(true);

  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));

  display::Screen* screen = display::Screen::GetScreen();
  // TODO(sergeyu): Move the notification to the display being captured when
  // per-display screen capture is supported.
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();

  // Place the bar in the center of the bottom of the display.
  gfx::Size size = widget->non_client_view()->GetPreferredSize();
  gfx::Rect bounds(
      work_area.x() + work_area.width() / 2 - size.width() / 2,
      work_area.y() + work_area.height() - size.height(),
      size.width(), size.height());
  widget->SetBounds(bounds);
  widget->Show();
  // This has to be called after Show() to have effect.
  widget->SetOpacity(kWindowAlphaValue);
  widget->SetVisibleOnAllWorkspaces(true);

#if defined(OS_WIN)
  return gfx::NativeViewId(views::HWNDForWidget(widget));
#else
  return 0;
#endif
}

gfx::Size ScreenCaptureNotificationUIViews::CalculatePreferredSize() const {
  gfx::Size grip_size = gripper_->GetPreferredSize();
  gfx::Size label_size = label_->GetPreferredSize();
  gfx::Size stop_button_size = stop_button_->GetPreferredSize();
  gfx::Size hide_link_size = hide_link_->GetPreferredSize();
  int width = kHorizontalMargin * 3 + grip_size.width() + label_size.width() +
      stop_button_size.width() + hide_link_size.width();
  width = std::max(width, kMinimumWidth);
  width = std::min(width, kMaximumWidth);
  return gfx::Size(width, std::max(label_size.height(),
                                   std::max(hide_link_size.height(),
                                            stop_button_size.height())));
}

void ScreenCaptureNotificationUIViews::Layout() {
  gfx::Rect grip_rect(gripper_->GetPreferredSize());
  grip_rect.set_y((bounds().height() - grip_rect.height()) / 2);
  gripper_->SetBoundsRect(grip_rect);

  gfx::Rect stop_button_rect(stop_button_->GetPreferredSize());
  gfx::Rect hide_link_rect(hide_link_->GetPreferredSize());

  hide_link_rect.set_x(bounds().width() - hide_link_rect.width());
  hide_link_rect.set_y((bounds().height() - hide_link_rect.height()) / 2);
  hide_link_->SetBoundsRect(hide_link_rect);

  stop_button_rect.set_x(
      hide_link_rect.x() - kHorizontalMargin - stop_button_rect.width());
  stop_button_->SetBoundsRect(stop_button_rect);

  gfx::Rect label_rect;
  label_rect.set_x(grip_rect.right() + kHorizontalMargin);
  label_rect.set_width(
      stop_button_rect.x() - kHorizontalMargin - label_rect.x());
  label_rect.set_height(bounds().height());
  label_->SetBoundsRect(label_rect);

  client_view_->set_client_rect(gfx::Rect(
      stop_button_rect.x(), stop_button_rect.y(),
      stop_button_rect.width() + kHorizontalMargin + hide_link_rect.width(),
      std::max(stop_button_rect.height(), hide_link_rect.height())));
}

void ScreenCaptureNotificationUIViews::DeleteDelegate() {
  NotifyStopped();
}

views::ClientView* ScreenCaptureNotificationUIViews::CreateClientView(
    views::Widget* widget) {
  DCHECK(!client_view_);
  client_view_ = new NotificationBarClientView(widget, this);
  return client_view_;
}

views::NonClientFrameView*
ScreenCaptureNotificationUIViews::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* frame = new views::BubbleFrameView(
      gfx::Insets(), gfx::Insets(kPaddingVertical, kPaddingHorizontal,
                                 kPaddingVertical, kPaddingHorizontal));
  SkColor color = widget->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  frame->SetBubbleBorder(std::unique_ptr<views::BubbleBorder>(
      new views::BubbleBorder(views::BubbleBorder::NONE,
                              views::BubbleBorder::SMALL_SHADOW, color)));
  return frame;
}

base::string16 ScreenCaptureNotificationUIViews::GetWindowTitle() const {
  return text_;
}

bool ScreenCaptureNotificationUIViews::ShouldShowWindowTitle() const {
  return false;
}

bool ScreenCaptureNotificationUIViews::ShouldShowCloseButton() const {
  return false;
}

bool ScreenCaptureNotificationUIViews::CanActivate() const {
  // When the window is visible, it can be activated so the mouse clicks
  // can be sent to the window; when the window is minimized, we don't want it
  // to activate, otherwise it sometimes does not show properly on Windows.
  return GetWidget() && GetWidget()->IsVisible();
}

void ScreenCaptureNotificationUIViews::ButtonPressed(views::Button* sender,
                                                     const ui::Event& event) {
  NotifyStopped();
}

void ScreenCaptureNotificationUIViews::LinkClicked(views::Link* source,
                                                   int event_flags) {
  GetWidget()->Minimize();
}

void ScreenCaptureNotificationUIViews::NotifyStopped() {
  if (!stop_callback_.is_null()) {
    base::Closure callback = stop_callback_;
    stop_callback_.Reset();
    callback.Run();
  }
}

}  // namespace

std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(const base::string16& text) {
  return std::unique_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUIViews(text));
}
