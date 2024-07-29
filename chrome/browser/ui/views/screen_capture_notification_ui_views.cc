// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/background.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/win/shell.h"
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
  METADATA_HEADER(NotificationBarClientView, views::ClientView)

 public:
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

BEGIN_METADATA(NotificationBarClientView)
ADD_PROPERTY_METADATA(gfx::Rect, ClientRect)
END_METADATA

class ScreenCaptureNotificationUIViews : public views::WidgetDelegateView,
                                         public views::ViewObserver {
  METADATA_HEADER(ScreenCaptureNotificationUIViews, views::WidgetDelegateView)

 public:
  ScreenCaptureNotificationUIViews(
      const std::u16string& text,
      content::WebContents* capturing_web_contents,
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback);
  ScreenCaptureNotificationUIViews(const ScreenCaptureNotificationUIViews&) =
      delete;
  ScreenCaptureNotificationUIViews& operator=(
      const ScreenCaptureNotificationUIViews&) = delete;
  ~ScreenCaptureNotificationUIViews() override;

  // views::WidgetDelegateView:
  views::ClientView* CreateClientView(views::Widget* widget) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  // Helper to call |stop_callback_|.
  void NotifyStopped();
  // Helper to call |source_callback_|.
  void NotifySourceChange();

  const base::WeakPtr<content::WebContents> capturing_web_contents_;
  base::OnceClosure stop_callback_;
  content::MediaStreamUI::SourceCallback source_callback_;
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      view_observations_{this};
  raw_ptr<NotificationBarClientView> client_view_ = nullptr;
  raw_ptr<views::View> source_button_ = nullptr;
  raw_ptr<views::View> stop_button_ = nullptr;
  raw_ptr<views::View> hide_link_ = nullptr;
};

// ScreenCaptureNotificationUI implementation using Views.
class ScreenCaptureNotificationUIImpl : public ScreenCaptureNotificationUI {
 public:
  ScreenCaptureNotificationUIImpl(const std::u16string& text,
                                  content::WebContents* capturing_web_contents);
  ScreenCaptureNotificationUIImpl(const ScreenCaptureNotificationUIImpl&) =
      delete;
  ScreenCaptureNotificationUIImpl& operator=(
      const ScreenCaptureNotificationUIImpl&) = delete;
  ~ScreenCaptureNotificationUIImpl() override = default;

  // ScreenCaptureNotificationUI override:
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

 private:
  // Helper to set window id to parent browser window id for task bar grouping.
#if BUILDFLAG(IS_WIN)
  void SetWindowsAppId(views::Widget* widget);
#endif

  std::u16string text_;
  base::WeakPtr<content::WebContents> capturing_web_contents_;
  std::unique_ptr<views::Widget> widget_;
};

ScreenCaptureNotificationUIViews::ScreenCaptureNotificationUIViews(
    const std::u16string& text,
    content::WebContents* capturing_web_contents,
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback)
    : capturing_web_contents_(capturing_web_contents
                                  ? capturing_web_contents->GetWeakPtr()
                                  : nullptr),
      stop_callback_(std::move(stop_callback)),
      source_callback_(std::move(source_callback)) {
  SetShowCloseButton(false);
  SetShowTitle(false);
  SetTitle(text);

  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(
      base::BindOnce(&ScreenCaptureNotificationUIViews::NotifyStopped,
                     base::Unretained(this)));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kHorizontalMargin));

  auto gripper = std::make_unique<views::ImageView>();
  gripper->SetImage(
      ui::ImageModel::FromResourceId(IDR_SCREEN_CAPTURE_NOTIFICATION_GRIP));
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

  if (source_callback_.is_null()) {
    source_button_->SetVisible(false);
  }

  std::u16string stop_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  auto stop_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ScreenCaptureNotificationUIViews::NotifyStopped,
                          base::Unretained(this)),
      stop_text);
  stop_button->SetStyle(ui::ButtonStyle::kProminent);
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
  view_observations_.AddObservation(source_button_.get());
  view_observations_.AddObservation(stop_button_.get());
  view_observations_.AddObservation(hide_link_.get());

  SetBackground(views::CreateThemedSolidBackground(ui::kColorDialogBackground));
}

ScreenCaptureNotificationUIViews::~ScreenCaptureNotificationUIViews() {
  source_callback_.Reset();
  stop_callback_.Reset();
}

views::ClientView* ScreenCaptureNotificationUIViews::CreateClientView(
    views::Widget* widget) {
  DCHECK(!client_view_);
  client_view_ = new NotificationBarClientView(widget, this);
  // To prevent client_view_ from dangling, we observe it for deletion.
  view_observations_.AddObservation(client_view_.get());
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
    views::View* observed_view) {
  if (observed_view == client_view_.get()) {
    return;
  }
  gfx::Rect client_rect = source_button_->bounds();
  client_rect.Union(stop_button_->bounds());
  client_rect.Union(hide_link_->bounds());
  client_view_->SetClientRect(client_rect);
}

void ScreenCaptureNotificationUIViews::OnViewIsDeleting(
    views::View* observed_view) {
  if (observed_view == client_view_.get()) {
    client_view_ = nullptr;
  }
}

void ScreenCaptureNotificationUIViews::NotifySourceChange() {
  if (!source_callback_.is_null()) {
    // CSC is only supported for tab-capture, so setting it to `false` is the
    // correct behavior so long as we don't support cross-surface-type
    // switching.
    source_callback_.Run(content::DesktopMediaID(),
                         /*captured_surface_control_active=*/false);
  }
}

void ScreenCaptureNotificationUIViews::NotifyStopped() {
  if (!stop_callback_.is_null()) {
    std::move(stop_callback_).Run();
  }
}

BEGIN_METADATA(ScreenCaptureNotificationUIViews)
END_METADATA

ScreenCaptureNotificationUIImpl::ScreenCaptureNotificationUIImpl(
    const std::u16string& text,
    content::WebContents* capturing_web_contents)
    : text_(text),
      capturing_web_contents_(capturing_web_contents
                                  ? capturing_web_contents->GetWeakPtr()
                                  : nullptr) {}

gfx::NativeViewId ScreenCaptureNotificationUIImpl::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  if (widget_) {
    return 0;
  }

  widget_ = std::make_unique<views::Widget>();
  auto screen_capture_notification_ui_views =
      std::make_unique<ScreenCaptureNotificationUIViews>(
          text_, capturing_web_contents_.get(), std::move(stop_callback),
          std::move(source_callback));

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = screen_capture_notification_ui_views.release();
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

  widget_->set_frame_type(views::Widget::FrameType::kForceCustom);
  widget_->Init(std::move(params));

  display::Screen* screen = display::Screen::GetScreen();
  // TODO(sergeyu): Move the notification to the display being captured when
  // per-display screen capture is supported.
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();

  // Place the bar in the center of the bottom of the display.
  gfx::Size size = widget_->non_client_view()->GetPreferredSize();
  gfx::Rect bounds(work_area.x() + work_area.width() / 2 - size.width() / 2,
                   work_area.y() + work_area.height() - size.height(),
                   size.width(), size.height());
  widget_->SetBounds(bounds);

#if BUILDFLAG(IS_WIN)
  SetWindowsAppId(widget_.get());
#endif

  if (media_ids.empty() ||
      media_ids.front().type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    // Focus the notification widget if sharing a screen.
    widget_->Show();
  } else {
    // Do not focus the notification widget if sharing a window.
    widget_->ShowInactive();
  }
  // This has to be called after Show() to have effect.
  widget_->SetOpacity(kWindowAlphaValue);
  widget_->SetVisibleOnAllWorkspaces(true);

  return 0;
}

#if BUILDFLAG(IS_WIN)
void ScreenCaptureNotificationUIImpl::SetWindowsAppId(views::Widget* widget) {
  if (!capturing_web_contents_) {
    return;
  }
  Browser* browser = chrome::FindBrowserWithTab(capturing_web_contents_.get());
  // Can be nullptr from extension background page call.
  if (!browser) {
    return;
  }
  const base::FilePath profile_path = browser->profile()->GetPath();
  std::wstring app_user_model_id =
      browser->is_type_app()
          ? shell_integration::win::GetAppUserModelIdForApp(
                base::UTF8ToWide(browser->app_name()), profile_path)
          : shell_integration::win::GetAppUserModelIdForBrowser(profile_path);
  if (!app_user_model_id.empty()) {
    ui::win::SetAppIdForWindow(app_user_model_id, views::HWNDForWidget(widget));
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(
    const std::u16string& text,
    content::WebContents* capturing_web_contents) {
  return std::make_unique<ScreenCaptureNotificationUIImpl>(
      text, capturing_web_contents);
}
