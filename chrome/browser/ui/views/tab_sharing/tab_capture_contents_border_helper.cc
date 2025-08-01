// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

constexpr int kMinContentsBorderWidth = 20;
constexpr int kMinContentsBorderHeight = 20;

class BorderView : public views::View {
 public:
  BorderView() = default;
  BorderView(const BorderView&) = delete;
  BorderView& operator=(const BorderView&) = delete;
  ~BorderView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    constexpr int kContentsBorderThickness = 5;
    SetBorder(views::CreateSolidBorder(
        kContentsBorderThickness,
        GetColorProvider()->GetColor(kColorCapturedTabContentsBorder)));
  }
};
}  // namespace

DEFINE_USER_DATA(TabCaptureContentsBorderHelper);

// static:
TabCaptureContentsBorderHelper* TabCaptureContentsBorderHelper::From(
    tabs::TabInterface* tab_interface) {
  return Get(tab_interface->GetUnownedUserDataHost());
}

TabCaptureContentsBorderHelper::TabCaptureContentsBorderHelper(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  tab_will_detach_subscription_ = tab_interface_->RegisterWillDetach(
      base::BindRepeating(&TabCaptureContentsBorderHelper::TabWillDetach,
                          base::Unretained(this)));
}

TabCaptureContentsBorderHelper::~TabCaptureContentsBorderHelper() = default;

void TabCaptureContentsBorderHelper::OnCapturerAdded(
    CaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!base::Contains(session_to_bounds_, capture_session_id));

  session_to_bounds_[capture_session_id] = std::nullopt;

  Update();
}

void TabCaptureContentsBorderHelper::OnCapturerRemoved(
    CaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/40213800): Destroy widget when the size of
  // `session_to_bounds_` hits 0. Same for `this`.
  session_to_bounds_.erase(capture_session_id);

  Update();
}

void TabCaptureContentsBorderHelper::VisibilityUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Update();
}

void TabCaptureContentsBorderHelper::OnRegionCaptureRectChanged(
    CaptureSessionId capture_session_id,
    const std::optional<gfx::Rect>& region_capture_rect) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(base::Contains(session_to_bounds_, capture_session_id));

  if (region_capture_rect &&
      region_capture_rect->width() >= kMinContentsBorderWidth &&
      region_capture_rect->height() >= kMinContentsBorderHeight) {
    session_to_bounds_[capture_session_id] = region_capture_rect;
  } else {
    session_to_bounds_[capture_session_id] = std::nullopt;
  }

  UpdateBlueBorderLocation();
}

void TabCaptureContentsBorderHelper::InitContentsBorderWidget() {
  Browser* const browser =
      tab_interface_->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();
  if (!browser) {
    return;
  }

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || browser_view->contents_border_widget()) {
    return;
  }

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  views::Widget* frame = browser_view->contents_web_view()->GetWidget();
  params.parent = frame->GetNativeView();
  params.context = frame->GetNativeWindow();
  // Make the widget non-top level.
  params.child = true;
  params.name = "TabSharingContentsBorder";
  params.remove_standard_frame = true;
  // Let events go through to underlying view.
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
#if BUILDFLAG(IS_WIN)
  params.native_widget = new views::NativeWidgetAura(widget);
#endif  // BUILDFLAG(IS_WIN)

  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<BorderView>());
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->SetOpacity(0.50f);

  // TODO(crbug.com/40207590): Associate each captured tab with its own widget.
  // Otherwise, if tab A captures B, and tab C captures D, and all are in
  // the same browser window, then either the A<-B or C<-D sessions ending,
  // hides the widget, and there's no good way of avoiding it (other than
  // associating distinct captured tabs with their own border).
  // After this fix, capturing a given tab X twice will still yield one widget.
  browser_view->set_contents_border_widget(widget);
}

void TabCaptureContentsBorderHelper::Update() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS)
  // The blue border behavior used to be problematic on ChromeOS - see
  // crbug.com/1320262 and crbug.com/1030925. This check serves as a means of
  // flag-disabling this feature in case of possible future regressions.
  if (!base::FeatureList::IsEnabled(features::kTabCaptureBlueBorderCrOS)) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  Browser* const browser =
      tab_interface_->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }

  const bool tab_visible = tab_interface_->IsActivated();
  const bool contents_border_needed =
      tab_visible && !session_to_bounds_.empty();

  if (!browser_view->contents_border_widget()) {
    if (!contents_border_needed) {
      return;
    }
    InitContentsBorderWidget();
  }

  views::Widget* const contents_border_widget =
      browser_view->contents_border_widget();

  if (contents_border_needed) {
    UpdateBlueBorderLocation();
    contents_border_widget->Show();
  } else {
    contents_border_widget->Hide();
  }
}

void TabCaptureContentsBorderHelper::UpdateBlueBorderLocation() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!session_to_bounds_.empty()) << "No blue border should be shown.";
  Browser* const browser =
      tab_interface_->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->contents_border_widget()) {
    return;
  }

  browser_view->SetContentBorderBounds(GetBlueBorderLocation());
}

std::optional<gfx::Rect> TabCaptureContentsBorderHelper::GetBlueBorderLocation()
    const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!session_to_bounds_.empty()) << "No blue border should be shown.";

  // The border should only track the cropped-to contents when there is exactly
  // one capture session. If there are more, fall back on drawing the border
  // around the entire tab.
  return (session_to_bounds_.size() == 1u) ? session_to_bounds_.begin()->second
                                           : std::nullopt;
}

void TabCaptureContentsBorderHelper::TabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  session_to_bounds_.clear();
  Update();
}
