// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include "base/macros.h"
#include "base/scoped_observer.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif

namespace {

const int kHorizontalMargin = 10;
const float kWindowAlphaValue = 0.96f;

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

  // views::ClientView:
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

// ScreenCaptureNotificationUI implementation using Views.
class ScreenCaptureNotificationUIViews : public ScreenCaptureNotificationUI,
                                         public views::WidgetDelegateView,
                                         public views::ButtonListener,
                                         public views::LinkListener,
                                         public views::ViewObserver {
 public:
  explicit ScreenCaptureNotificationUIViews(const base::string16& text);
  ~ScreenCaptureNotificationUIViews() override;

  // ScreenCaptureNotificationUI:
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;

  // views::WidgetDelegateView:
  void DeleteDelegate() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool CanActivate() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

 private:
  // Helper to call |stop_callback_|.
  void NotifyStopped();
  // Helper to call |source_callback_|.
  void NotifySourceChange();

  base::OnceClosure stop_callback_;
  content::MediaStreamUI::SourceCallback source_callback_;
  ScopedObserver<views::View, views::ViewObserver> bounds_observer_{this};
  NotificationBarClientView* client_view_ = nullptr;
  views::ImageView* gripper_ = nullptr;
  views::Label* label_ = nullptr;
  views::Button* source_button_ = nullptr;
  views::Button* stop_button_ = nullptr;
  views::Link* hide_link_ = nullptr;
  const base::string16 text_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIViews);
};

ScreenCaptureNotificationUIViews::ScreenCaptureNotificationUIViews(
    const base::string16& text)
    : text_(text) {
  set_owned_by_client();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kHorizontalMargin));

  auto gripper = std::make_unique<views::ImageView>();
  gripper->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SCREEN_CAPTURE_NOTIFICATION_GRIP));
  gripper_ = AddChildView(std::move(gripper));

  label_ = AddChildView(std::make_unique<views::Label>());

  base::string16 source_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_SOURCE);
  auto source_button =
      views::MdTextButton::CreateSecondaryUiButton(this, source_text);
  source_button_ = AddChildView(std::move(source_button));

  base::string16 stop_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  auto stop_button =
      views::MdTextButton::CreateSecondaryUiBlueButton(this, stop_text);
  stop_button_ = AddChildView(std::move(stop_button));

  // TODO(jiayl): IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON is used for the need to
  // merge to M34. Change it to a new IDS_ after the merge.
  auto hide_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON));
  hide_link->set_listener(this);
  hide_link->SetUnderline(false);
  hide_link_ = AddChildView(std::move(hide_link));

  // The client rect for NotificationBarClientView uses the bounds for the
  // following views.
  bounds_observer_.Add(source_button_);
  bounds_observer_.Add(stop_button_);
  bounds_observer_.Add(hide_link_);
}

ScreenCaptureNotificationUIViews::~ScreenCaptureNotificationUIViews() {
  source_callback_.Reset();
  stop_callback_.Reset();
  delete GetWidget();
}

gfx::NativeViewId ScreenCaptureNotificationUIViews::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  if (GetWidget())
    return 0;

  stop_callback_ = std::move(stop_callback);
  source_callback_ = std::move(source_callback);

  if (source_callback_.is_null())
    source_button_->SetVisible(false);

  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetText(text_);

  views::Widget* widget = new views::Widget;

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.remove_standard_frame = true;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = "ScreenCaptureNotificationUIViews";

#if defined(OS_CHROMEOS)
  // TODO(sergeyu): The notification bar must be shown on the monitor that's
  // being captured. Make sure it's always the case. Currently we always capture
  // the primary monitor.
  params.context = ash::Shell::GetPrimaryRootWindow();
#endif

  widget->set_frame_type(views::Widget::FrameType::kForceCustom);
  widget->Init(std::move(params));

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

  return 0;
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
  constexpr auto kPadding = gfx::Insets(5, 10);
  views::BubbleFrameView* frame =
      new views::BubbleFrameView(gfx::Insets(), kPadding);
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
  if (sender == stop_button_) {
    NotifyStopped();
  } else {
    DCHECK_EQ(source_button_, sender);
    NotifySourceChange();
  }
}

void ScreenCaptureNotificationUIViews::LinkClicked(views::Link* source,
                                                   int event_flags) {
  GetWidget()->Minimize();
}

void ScreenCaptureNotificationUIViews::OnViewBoundsChanged(
    View* observed_view) {
  gfx::Rect client_rect = source_button_->bounds();
  client_rect.Union(stop_button_->bounds());
  client_rect.Union(hide_link_->bounds());
  client_view_->set_client_rect(client_rect);
}

void ScreenCaptureNotificationUIViews::NotifySourceChange() {
  if (!source_callback_.is_null())
    source_callback_.Run(content::DesktopMediaID());
}

void ScreenCaptureNotificationUIViews::NotifyStopped() {
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();
}

}  // namespace

std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(const base::string16& text) {
  return std::unique_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUIViews(text));
}
