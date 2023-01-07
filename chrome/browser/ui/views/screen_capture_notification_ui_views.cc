// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/scoped_multi_source_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#endif

namespace {

const int kHorizontalMargin = 10;
const float kWindowAlphaValue = 0.96f;

// A ClientView that overrides NonClientHitTest() so that the whole window area
// acts as a window caption, except a rect specified using SetClientRect().
// ScreenCaptureNotificationUIViews uses this class to make the notification bar
// draggable.
class NotificationBarClientView : public views::ClientView {
 public:
  METADATA_HEADER(NotificationBarClientView);
  NotificationBarClientView(views::Widget* widget, views::View* view)
      : views::ClientView(widget, view) {
  }
  NotificationBarClientView(const NotificationBarClientView&) = delete;
  NotificationBarClientView& operator=(const NotificationBarClientView&) =
      delete;
  ~NotificationBarClientView() override = default;

  void SetClientRect(const gfx::Rect& rect) {
    if (rect_ == rect)
      return;
    rect_ = rect;
    OnPropertyChanged(&rect_, views::kPropertyEffectsNone);
  }
  gfx::Rect GetClientRect() const { return rect_; }

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
};

BEGIN_METADATA(NotificationBarClientView, views::ClientView)
ADD_PROPERTY_METADATA(gfx::Rect, ClientRect)
END_METADATA

// ScreenCaptureNotificationUI implementation using Views.
class ScreenCaptureNotificationUIViews : public ScreenCaptureNotificationUI,
                                         public views::WidgetDelegateView,
                                         public views::ViewObserver {
 public:
  METADATA_HEADER(ScreenCaptureNotificationUIViews);
  explicit ScreenCaptureNotificationUIViews(const std::u16string& text);
  ScreenCaptureNotificationUIViews(const ScreenCaptureNotificationUIViews&) =
      delete;
  ScreenCaptureNotificationUIViews& operator=(
      const ScreenCaptureNotificationUIViews&) = delete;
  ~ScreenCaptureNotificationUIViews() override;

  // ScreenCaptureNotificationUI:
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

  // views::WidgetDelegateView:
  views::ClientView* CreateClientView(views::Widget* widget) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

 private:
  // Helper to call |stop_callback_|.
  void NotifyStopped();
  // Helper to call |source_callback_|.
  void NotifySourceChange();

  base::OnceClosure stop_callback_;
  content::MediaStreamUI::SourceCallback source_callback_;
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      bounds_observations_{this};
  raw_ptr<NotificationBarClientView, DanglingUntriaged> client_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> source_button_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> stop_button_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> hide_link_ = nullptr;
};

ScreenCaptureNotificationUIViews::ScreenCaptureNotificationUIViews(
    const std::u16string& text) {
  SetShowCloseButton(false);
  SetShowTitle(false);
  SetTitle(text);

  // TODO(pbos): Investigate if this can be SetOwnedByWidget(true) and get rid
  // of `delete GetWidget();` in the destructor.
  set_owned_by_client();
  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(
      base::BindOnce(&ScreenCaptureNotificationUIViews::NotifyStopped,
                     base::Unretained(this)));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kHorizontalMargin));

  auto gripper = std::make_unique<views::ImageView>();
  gripper->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SCREEN_CAPTURE_NOTIFICATION_GRIP));
  AddChildView(std::move(gripper));

  auto label = std::make_unique<views::Label>(text);
  label->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));

  std::u16string source_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_SOURCE);
  source_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ScreenCaptureNotificationUIViews::NotifySourceChange,
                          base::Unretained(this)),
      source_text));

  std::u16string stop_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  auto stop_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ScreenCaptureNotificationUIViews::NotifyStopped,
                          base::Unretained(this)),
      stop_text);
  stop_button->SetProminent(true);
  stop_button_ = AddChildView(std::move(stop_button));

  auto hide_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_HIDE));
  hide_link->SetCallback(base::BindRepeating(
      [](ScreenCaptureNotificationUIViews* view) {
        view->GetWidget()->Minimize();
      },
      base::Unretained(this)));
  hide_link_ = AddChildView(std::move(hide_link));

  // The client rect for NotificationBarClientView uses the bounds for the
  // following views.
  bounds_observations_.AddObservation(source_button_.get());
  bounds_observations_.AddObservation(stop_button_.get());
  bounds_observations_.AddObservation(hide_link_.get());
}

ScreenCaptureNotificationUIViews::~ScreenCaptureNotificationUIViews() {
  source_callback_.Reset();
  stop_callback_.Reset();
  delete GetWidget();
}

gfx::NativeViewId ScreenCaptureNotificationUIViews::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  if (GetWidget())
    return 0;

  stop_callback_ = std::move(stop_callback);
  source_callback_ = std::move(source_callback);

  if (source_callback_.is_null())
    source_button_->SetVisible(false);

  views::Widget* widget = new views::Widget;

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.remove_standard_frame = true;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = "ScreenCaptureNotificationUIViews";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(sergeyu): The notification bar must be shown on the monitor that's
  // being captured. Make sure it's always the case. Currently we always capture
  // the primary monitor.
  params.context = ash::Shell::GetPrimaryRootWindow();
#endif

  widget->set_frame_type(views::Widget::FrameType::kForceCustom);
  widget->Init(std::move(params));

  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorDialogBackground)));

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
  if (media_ids.empty() ||
      media_ids.front().type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    // Focus the notification widget if sharing a screen.
    widget->Show();
  } else {
    // Do not focus the notification widget if sharing a window.
    widget->ShowInactive();
  }
  // This has to be called after Show() to have effect.
  widget->SetOpacity(kWindowAlphaValue);
  widget->SetVisibleOnAllWorkspaces(true);

  return 0;
}

views::ClientView* ScreenCaptureNotificationUIViews::CreateClientView(
    views::Widget* widget) {
  DCHECK(!client_view_);
  client_view_ = new NotificationBarClientView(widget, this);
  return client_view_;
}

std::unique_ptr<views::NonClientFrameView>
ScreenCaptureNotificationUIViews::CreateNonClientFrameView(
    views::Widget* widget) {
  constexpr auto kPadding = gfx::Insets::VH(5, 10);
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), kPadding);
  frame->SetBubbleBorder(std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW));
  return frame;
}

void ScreenCaptureNotificationUIViews::OnViewBoundsChanged(
    View* observed_view) {
  gfx::Rect client_rect = source_button_->bounds();
  client_rect.Union(stop_button_->bounds());
  client_rect.Union(hide_link_->bounds());
  client_view_->SetClientRect(client_rect);
}

void ScreenCaptureNotificationUIViews::NotifySourceChange() {
  if (!source_callback_.is_null())
    source_callback_.Run(content::DesktopMediaID());
}

void ScreenCaptureNotificationUIViews::NotifyStopped() {
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();
}

BEGIN_METADATA(ScreenCaptureNotificationUIViews, views::WidgetDelegateView)
END_METADATA

}  // namespace

std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(const std::u16string& text) {
  return std::unique_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUIViews(text));
}
