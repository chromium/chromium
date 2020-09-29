// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include <algorithm>
#include <utility>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_platform_specific.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/views_delegate.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"
#include "ui/views/window/window_shape.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "ui/views/controls/menu/menu_runner.h"
#endif

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif

using content::WebContents;

namespace {

class CaptionButtonBackgroundImageSource : public gfx::CanvasImageSource {
 public:
  CaptionButtonBackgroundImageSource(const gfx::ImageSkia& bg_image,
                                     int source_x,
                                     int source_y,
                                     int dest_width,
                                     int dest_height,
                                     bool draw_mirrored)
      : gfx::CanvasImageSource(bg_image.isNull() ? gfx::Size(1, 1)
                                                 : bg_image.size()),
        bg_image_(bg_image),
        source_x_(source_x),
        source_y_(source_y),
        dest_width_(dest_width),
        dest_height_(dest_height),
        draw_mirrored_(draw_mirrored) {}

  ~CaptionButtonBackgroundImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override {
    if (bg_image_.isNull())
      return;

    gfx::ScopedCanvas scoped_canvas(canvas);

    if (draw_mirrored_) {
      canvas->Translate(gfx::Vector2d(dest_width_, 0));
      canvas->Scale(-1, 1);
    }

    canvas->TileImageInt(bg_image_, source_x_, source_y_, 0, 0, dest_width_,
                         dest_height_);
  }

 private:
  const gfx::ImageSkia bg_image_;
  int source_x_, source_y_;
  int dest_width_, dest_height_;
  bool draw_mirrored_;

  DISALLOW_COPY_AND_ASSIGN(CaptionButtonBackgroundImageSource);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, public:

const char OpaqueBrowserFrameView::kClassName[] = "OpaqueBrowserFrameView";

OpaqueBrowserFrameView::OpaqueBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view,
    OpaqueBrowserFrameViewLayout* layout)
    : BrowserNonClientFrameView(frame, browser_view),
      layout_(layout),
      window_icon_(nullptr),
      window_title_(nullptr),
      frame_background_(new views::FrameBackground()) {
  layout_->set_delegate(this);
  SetLayoutManager(std::unique_ptr<views::LayoutManager>(layout_));

  platform_observer_ =
      OpaqueBrowserFrameViewPlatformSpecific::Create(this, layout_);
}

OpaqueBrowserFrameView::~OpaqueBrowserFrameView() {}

void OpaqueBrowserFrameView::InitViews() {
  if (GetFrameButtonStyle() == FrameButtonStyle::kMdButton) {
    minimize_button_ = CreateFrameCaptionButton(
        views::CAPTION_BUTTON_ICON_MINIMIZE, HTMINBUTTON,
        views::kWindowControlMinimizeIcon);
    maximize_button_ = CreateFrameCaptionButton(
        views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, HTMAXBUTTON,
        views::kWindowControlMaximizeIcon);
    restore_button_ =
        CreateFrameCaptionButton(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
                                 HTMAXBUTTON, views::kWindowControlRestoreIcon);
    close_button_ =
        CreateFrameCaptionButton(views::CAPTION_BUTTON_ICON_CLOSE, HTMAXBUTTON,
                                 views::kWindowControlCloseIcon);
  } else if (GetFrameButtonStyle() == FrameButtonStyle::kImageButton) {
    minimize_button_ =
        CreateImageButton(IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
                          IDR_MINIMIZE_BUTTON_MASK, VIEW_ID_MINIMIZE_BUTTON);
    maximize_button_ =
        CreateImageButton(IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
                          IDR_MAXIMIZE_BUTTON_MASK, VIEW_ID_MAXIMIZE_BUTTON);
    restore_button_ =
        CreateImageButton(IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
                          IDR_RESTORE_BUTTON_MASK, VIEW_ID_RESTORE_BUTTON);
    close_button_ =
        CreateImageButton(IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
                          IDR_CLOSE_BUTTON_MASK, VIEW_ID_CLOSE_BUTTON);
  }
  InitWindowCaptionButton(
      minimize_button_,
      base::BindRepeating(&BrowserFrame::Minimize, base::Unretained(frame())),
      IDS_ACCNAME_MINIMIZE, VIEW_ID_MINIMIZE_BUTTON);
  InitWindowCaptionButton(
      maximize_button_,
      base::BindRepeating(&BrowserFrame::Maximize, base::Unretained(frame())),
      IDS_ACCNAME_MAXIMIZE, VIEW_ID_MAXIMIZE_BUTTON);
  InitWindowCaptionButton(
      restore_button_,
      base::BindRepeating(&BrowserFrame::Restore, base::Unretained(frame())),
      IDS_ACCNAME_RESTORE, VIEW_ID_RESTORE_BUTTON);
  InitWindowCaptionButton(
      close_button_,
      base::BindRepeating(&BrowserFrame::CloseWithReason,
                          base::Unretained(frame()),
                          views::Widget::ClosedReason::kCloseButtonClicked),
      IDS_ACCNAME_CLOSE, VIEW_ID_CLOSE_BUTTON);

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(
        this, base::BindRepeating(&OpaqueBrowserFrameView::WindowIconPressed,
                                  base::Unretained(this)));
    window_icon_->set_is_light(true);
    window_icon_->SetID(VIEW_ID_WINDOW_ICON);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  web_app::AppBrowserController* controller =
      browser_view()->browser()->app_controller();
  if (controller) {
    set_web_app_frame_toolbar(AddChildView(
        std::make_unique<WebAppFrameToolbarView>(frame(), browser_view())));
  }

  // The window title appears above the web app frame toolbar (if present),
  // which surrounds the title with minimal-ui buttons on the left,
  // and other controls (such as the app menu button) on the right.
  window_title_ = new views::Label(browser_view()->GetWindowTitle());
  window_title_->SetVisible(browser_view()->ShouldShowWindowTitle());
  window_title_->SetSubpixelRenderingEnabled(false);
  window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_->SetID(VIEW_ID_WINDOW_TITLE);
  AddChildView(window_title_);
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return layout_->GetBoundsForTabStripRegion(tabstrip_minimum_size, width());
}

int OpaqueBrowserFrameView::GetTopInset(bool restored) const {
  return browser_view()->IsTabStripVisible()
             ? layout_->GetTabStripInsetsTop(restored)
             : layout_->NonClientTopHeight(restored);
}

int OpaqueBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

void OpaqueBrowserFrameView::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

gfx::Size OpaqueBrowserFrameView::GetMinimumSize() const {
  return layout_->GetMinimumSize(this);
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForClientView() const {
  return layout_->client_view_bounds();
}

gfx::Rect OpaqueBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return layout_->GetWindowBoundsForClientBounds(client_bounds);
}

int OpaqueBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  int super_component = BrowserNonClientFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE)
    return super_component;

  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  if (ShouldShowWindowIcon() && frame_component != HTCLIENT) {
    gfx::Rect sysmenu_rect(IconBounds());
    // In maximized mode we extend the rect to the screen corner to take
    // advantage of Fitts' Law.
    if (IsFrameCondensed())
      sysmenu_rect.SetRect(0, 0, sysmenu_rect.right(), sysmenu_rect.bottom());
    sysmenu_rect = GetMirroredRect(sysmenu_rect);
    if (sysmenu_rect.Contains(point))
      return HTSYSMENU;
  }

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_ && close_button_->GetVisible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (restore_button_ && restore_button_->GetVisible() &&
      restore_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;
  if (maximize_button_ && maximize_button_->GetVisible() &&
      maximize_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;
  if (minimize_button_ && minimize_button_->GetVisible() &&
      minimize_button_->GetMirroredBounds().Contains(point))
    return HTMINBUTTON;

  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate) {
    LOG(WARNING) << "delegate is null, returning safe default.";
    return HTCAPTION;
  }

  // In the window corners, the resize areas don't actually expand bigger, but
  // the 16 px at the end of each edge triggers diagonal resizing.
  constexpr int kResizeAreaCornerSize = 16;
  int window_component = GetHTComponentForFrame(
      point, FrameTopBorderThickness(false), FrameBorderThickness(false),
      kResizeAreaCornerSize, kResizeAreaCornerSize, delegate->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void OpaqueBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                           SkPath* window_mask) {
  DCHECK(window_mask);

  if (IsFrameCondensed())
    return;

  views::GetDefaultWindowMask(
      size, frame()->GetCompositor()->device_scale_factor(), window_mask);
}

void OpaqueBrowserFrameView::ResetWindowControls() {
  BrowserNonClientFrameView::ResetWindowControls();
  restore_button_->SetState(views::Button::STATE_NORMAL);
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  maximize_button_->SetState(views::Button::STATE_NORMAL);
  // The close button isn't affected by this constraint.
}

void OpaqueBrowserFrameView::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void OpaqueBrowserFrameView::UpdateWindowTitle() {
  if (!frame()->IsFullscreen() && ShouldShowWindowTitle()) {
    Layout();
    window_title_->SchedulePaint();
  }
}

void OpaqueBrowserFrameView::SizeConstraintsChanged() {}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::View overrides:

const char* OpaqueBrowserFrameView::GetClassName() const {
  return kClassName;
}

void OpaqueBrowserFrameView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTitleBar;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, TabIconView::TabContentsProvider implementation:

bool OpaqueBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

gfx::ImageSkia OpaqueBrowserFrameView::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate) {
    LOG(WARNING) << "delegate is null, returning safe default.";
    return gfx::ImageSkia();
  }
  return delegate->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, OpaqueBrowserFrameViewLayoutDelegate implementation:

bool OpaqueBrowserFrameView::ShouldShowWindowIcon() const {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return ShouldShowWindowTitleBar() && delegate &&
         delegate->ShouldShowWindowIcon();
}

bool OpaqueBrowserFrameView::ShouldShowWindowTitle() const {
  // |delegate| may be null if called from callback of InputMethodChanged while
  // a window is being destroyed.
  // See more discussion at http://crosbug.com/8958
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return ShouldShowWindowTitleBar() && delegate &&
         delegate->ShouldShowWindowTitle();
}

base::string16 OpaqueBrowserFrameView::GetWindowTitle() const {
  return frame()->widget_delegate()->GetWindowTitle();
}

int OpaqueBrowserFrameView::GetIconSize() const {
#if defined(OS_WIN)
  // This metric scales up if either the titlebar height or the titlebar font
  // size are increased.
  return display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON);
#else
  // The icon never shrinks below 16 px on a side.
  const int kIconMinimumSize = 16;
  return std::max(gfx::FontList().GetHeight(), kIconMinimumSize);
#endif
}

gfx::Size OpaqueBrowserFrameView::GetBrowserViewMinimumSize() const {
  return browser_view()->GetMinimumSize();
}

bool OpaqueBrowserFrameView::ShouldShowCaptionButtons() const {
  return ShouldShowWindowTitleBar();
}

bool OpaqueBrowserFrameView::IsRegularOrGuestSession() const {
  return browser_view()->IsRegularOrGuestSession();
}

bool OpaqueBrowserFrameView::IsMaximized() const {
  return frame()->IsMaximized();
}

bool OpaqueBrowserFrameView::IsMinimized() const {
  return frame()->IsMinimized();
}

bool OpaqueBrowserFrameView::IsFullscreen() const {
  return frame()->IsFullscreen();
}

bool OpaqueBrowserFrameView::IsTabStripVisible() const {
  return browser_view()->IsTabStripVisible();
}

bool OpaqueBrowserFrameView::IsToolbarVisible() const {
  return browser_view()->IsToolbarVisible() &&
         !browser_view()->toolbar()->GetPreferredSize().IsEmpty();
}

int OpaqueBrowserFrameView::GetTabStripHeight() const {
  return browser_view()->GetTabStripHeight();
}

gfx::Size OpaqueBrowserFrameView::GetTabstripMinimumSize() const {
  return browser_view()->tab_strip_region_view()->GetMinimumSize();
}

int OpaqueBrowserFrameView::GetTopAreaHeight() const {
  const int non_client_top_height = layout_->NonClientTopHeight(false);
  if (!browser_view()->IsTabStripVisible())
    return non_client_top_height;
  return std::max(
      non_client_top_height,
      GetBoundsForTabStripRegion(GetTabstripMinimumSize()).bottom() -
          GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
}

bool OpaqueBrowserFrameView::UseCustomFrame() const {
  return frame()->UseCustomFrame();
}

bool OpaqueBrowserFrameView::IsFrameCondensed() const {
  return BrowserNonClientFrameView::IsFrameCondensed() ||
         !ShouldShowCaptionButtons();
}

bool OpaqueBrowserFrameView::EverHasVisibleBackgroundTabShapes() const {
  return BrowserNonClientFrameView::EverHasVisibleBackgroundTabShapes();
}

OpaqueBrowserFrameView::FrameButtonStyle
OpaqueBrowserFrameView::GetFrameButtonStyle() const {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return FrameButtonStyle::kMdButton;
#else
  return FrameButtonStyle::kImageButton;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, protected:

// views::View:
void OpaqueBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT0("views.frame", "OpaqueBrowserFrameView::OnPaint");
  if (frame()->IsFullscreen())
    return;  // Nothing is visible, so don't bother to paint.

  const bool active = ShouldPaintAsActive();
  SkColor frame_color = GetFrameColor();
  window_title_->SetEnabledColor(
      GetCaptionColor(BrowserFrameActiveState::kUseCurrent));
  window_title_->SetBackgroundColor(frame_color);
  frame_background_->set_frame_color(frame_color);
  frame_background_->set_use_custom_frame(frame()->UseCustomFrame());
  frame_background_->set_is_active(active);
  frame_background_->set_incognito(browser_view()->IsIncognito());
  frame_background_->set_theme_image(GetFrameImage());
  const int y_inset =
      browser_view()->IsTabStripVisible()
          ? (ThemeProperties::kFrameHeightAboveTabs - GetTopInset(false))
          : 0;
  frame_background_->set_theme_image_y_inset(y_inset);
  frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
  frame_background_->set_top_area_height(GetTopAreaHeight());

  if (GetFrameButtonStyle() == FrameButtonStyle::kMdButton) {
    for (views::Button* button :
         {minimize_button_, maximize_button_, restore_button_, close_button_}) {
      DCHECK_EQ(std::string(views::FrameCaptionButton::kViewClassName),
                button->GetClassName());
      views::FrameCaptionButton* frame_caption_button =
          static_cast<views::FrameCaptionButton*>(button);
      frame_caption_button->set_paint_as_active(active);
      frame_caption_button->SetBackgroundColor(frame_color);
    }
  }

  if (IsFrameCondensed())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);

  // The window icon and title are painted by their respective views.
  /* TODO(pkasting):  If this window is active, we should also draw a drop
   * shadow on the title.  This is tricky, because we don't want to hardcode a
   * shadow color (since we want to work with various themes), but we can't
   * alpha-blend either (since the Windows text APIs don't really do this).
   * So we'd need to sample the background color at the right location and
   * synthesize a good shadow color. */

  // Custom tab bar mode draws the toolbar as a unified part of the titlebar, so
  // it shouldn't have a client edge.
  if (!browser_view()->toolbar()->custom_tab_bar())
    PaintClientEdge(canvas);
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, private:

views::Button* OpaqueBrowserFrameView::CreateFrameCaptionButton(
    views::CaptionButtonIcon icon_type,
    int ht_component,
    const gfx::VectorIcon& icon_image) {
  views::FrameCaptionButton* button =
      new views::FrameCaptionButton(nullptr, icon_type, ht_component);
  button->SetImage(button->icon(), views::FrameCaptionButton::ANIMATE_NO,
                   icon_image);
  return button;
}

views::Button* OpaqueBrowserFrameView::CreateImageButton(int normal_image_id,
                                                         int hot_image_id,
                                                         int pushed_image_id,
                                                         int mask_image_id,
                                                         ViewID view_id) {
  views::ImageButton* button = new views::ImageButton(nullptr);
  const ui::ThemeProvider* tp = frame()->GetThemeProvider();
  button->SetImage(views::Button::STATE_NORMAL,
                   tp->GetImageSkiaNamed(normal_image_id));
  button->SetImage(views::Button::STATE_HOVERED,
                   tp->GetImageSkiaNamed(hot_image_id));
  button->SetImage(views::Button::STATE_PRESSED,
                   tp->GetImageSkiaNamed(pushed_image_id));
  if (browser_view()->IsBrowserTypeNormal()) {
    // Get a custom processed version of the theme's background image so
    // that it appears to draw contiguously across all of the caption
    // buttons.
    const gfx::Size normal_image_size = GetThemeImageSize(normal_image_id);
    const gfx::ImageSkia processed_bg_image =
        GetProcessedBackgroundImageForCaptionButon(view_id, normal_image_size);

    // SetBackgroundImage immediately uses the provided ImageSkia pointer
    // (&processed_bg_image) to create a local copy, so it's safe for this
    // to be locally scoped.
    button->SetBackgroundImage(
        tp->GetColor(ThemeProperties::COLOR_CONTROL_BUTTON_BACKGROUND),
        (processed_bg_image.isNull() ? nullptr : &processed_bg_image),
        tp->GetImageSkiaNamed(mask_image_id));
  }
  return button;
}

void OpaqueBrowserFrameView::InitWindowCaptionButton(
    views::Button* button,
    views::Button::PressedCallback callback,
    int accessibility_string_id,
    ViewID view_id) {
  button->set_callback(std::move(callback));
  button->SetAccessibleName(l10n_util::GetStringUTF16(accessibility_string_id));
  button->SetID(view_id);
  AddChildView(button);
}

gfx::Size OpaqueBrowserFrameView::GetThemeImageSize(int image_id) {
  const ui::ThemeProvider* tp = frame()->GetThemeProvider();
  const gfx::ImageSkia* image = tp->GetImageSkiaNamed(image_id);

  return (image ? image->size() : gfx::Size());
}

int OpaqueBrowserFrameView::CalculateCaptionButtonBackgroundXOffset(
    ViewID view_id) {
  const int minimize_width = GetThemeImageSize(IDR_MINIMIZE).width();
  const int maximize_restore_width = GetThemeImageSize(IDR_MAXIMIZE).width();
  const int close_width = GetThemeImageSize(IDR_CLOSE).width();

  const bool is_rtl = base::i18n::IsRTL();

  switch (view_id) {
    case VIEW_ID_MINIMIZE_BUTTON:
      return (is_rtl ? close_width + maximize_restore_width : 0);
    case VIEW_ID_MAXIMIZE_BUTTON:
    case VIEW_ID_RESTORE_BUTTON:
      return (is_rtl ? close_width : minimize_width);
    case VIEW_ID_CLOSE_BUTTON:
      return (is_rtl ? 0 : minimize_width + maximize_restore_width);
    default:
      NOTREACHED();
      return 0;
  }
}

gfx::ImageSkia
OpaqueBrowserFrameView::GetProcessedBackgroundImageForCaptionButon(
    ViewID view_id,
    const gfx::Size& desired_size) {
  // We want the background image to tile contiguously across all of the
  // caption buttons, so we need to draw a subset of the background image,
  // with source offsets based on where this button is located relative to the
  // other caption buttons.  We also have to account for the image mirroring
  // that happens in RTL mode.  This is accomplished using a custom
  // ImageSource (defined at the top of the file).

  const ui::ThemeProvider* tp = frame()->GetThemeProvider();
  const gfx::ImageSkia* bg_image =
      tp->GetImageSkiaNamed(IDR_THEME_WINDOW_CONTROL_BACKGROUND);

  if (!bg_image)
    return gfx::ImageSkia();

  const bool is_rtl = base::i18n::IsRTL();
  const int bg_x_offset = CalculateCaptionButtonBackgroundXOffset(view_id);
  const int bg_y_offset = 0;
  std::unique_ptr<CaptionButtonBackgroundImageSource> source =
      std::make_unique<CaptionButtonBackgroundImageSource>(
          *bg_image, bg_x_offset, bg_y_offset, desired_size.width(),
          desired_size.height(), is_rtl);

  return gfx::ImageSkia(std::move(source), desired_size);
}

int OpaqueBrowserFrameView::FrameBorderThickness(bool restored) const {
  return layout_->FrameBorderThickness(restored);
}

int OpaqueBrowserFrameView::FrameTopBorderThickness(bool restored) const {
  return layout_->FrameTopBorderThickness(restored);
}

gfx::Rect OpaqueBrowserFrameView::IconBounds() const {
  return layout_->IconBounds();
}

void OpaqueBrowserFrameView::WindowIconPressed() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // TODO(pbos): Figure out / document why this is Linux only. This needs a
  // comment.
  views::MenuRunner menu_runner(frame()->GetSystemMenuModel(),
                                views::MenuRunner::HAS_MNEMONICS);
  menu_runner.RunMenuAt(
      browser_view()->GetWidget(), window_icon_->button_controller(),
      window_icon_->GetBoundsInScreen(), views::MenuAnchorPosition::kTopLeft,
      ui::MENU_SOURCE_MOUSE);
#endif
}

bool OpaqueBrowserFrameView::ShouldShowWindowTitleBar() const {
  // Do not show the custom title bar if the system title bar option is enabled.
  if (!frame()->UseCustomFrame())
    return false;

  // Do not show caption buttons if the window manager is forcefully providing a
  // title bar (e.g., in Ubuntu Unity, if the window is maximized).
  return !views::ViewsDelegate::GetInstance()->WindowManagerProvidesTitleBar(
      IsMaximized());
}

void OpaqueBrowserFrameView::PaintRestoredFrameBorder(
    gfx::Canvas* canvas) const {
  const ui::ThemeProvider* tp = GetThemeProvider();
  frame_background_->SetSideImages(
      tp->GetImageSkiaNamed(IDR_WINDOW_LEFT_SIDE),
      tp->GetImageSkiaNamed(IDR_WINDOW_TOP_CENTER),
      tp->GetImageSkiaNamed(IDR_WINDOW_RIGHT_SIDE),
      tp->GetImageSkiaNamed(IDR_WINDOW_BOTTOM_CENTER));
  frame_background_->SetCornerImages(
      tp->GetImageSkiaNamed(IDR_WINDOW_TOP_LEFT_CORNER),
      tp->GetImageSkiaNamed(IDR_WINDOW_TOP_RIGHT_CORNER),
      tp->GetImageSkiaNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER),
      tp->GetImageSkiaNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER));
  frame_background_->PaintRestored(canvas, this);

  // Note: When we don't have a toolbar, we need to draw some kind of bottom
  // edge here.  Because the App Window graphics we use for this have an
  // attached client edge and their sizing algorithm is a little involved, we do
  // all this in PaintRestoredClientEdge().
}

void OpaqueBrowserFrameView::PaintMaximizedFrameBorder(
    gfx::Canvas* canvas) const {
  frame_background_->set_maximized_top_inset(GetTopInset(true) -
                                             GetTopInset(false));
  frame_background_->PaintMaximized(canvas, this);
}

void OpaqueBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) const {
  const bool tabstrip_visible = browser_view()->IsTabStripVisible();
  const gfx::Rect client_bounds =
      layout_->CalculateClientAreaBounds(width(), height());

  // In maximized mode, the only edge to draw is the top one, so we're done.
  if (IsFrameCondensed())
    return;

  int y = client_bounds.y();
  const gfx::Rect toolbar_bounds = browser_view()->toolbar()->bounds();
  if (tabstrip_visible) {
    // The client edges start at the top of the toolbar.
    y += toolbar_bounds.y();
  }

  // For popup windows, draw location bar sides.
  const SkColor location_bar_border_color =
      browser_view()->toolbar()->location_bar()->GetOpaqueBorderColor();
  if (!tabstrip_visible && IsToolbarVisible()) {
    gfx::Rect side(client_bounds.x() - kClientEdgeThickness, y,
                   kClientEdgeThickness, toolbar_bounds.height());
    canvas->FillRect(side, location_bar_border_color);
    side.Offset(client_bounds.width() + kClientEdgeThickness, 0);
    canvas->FillRect(side, location_bar_border_color);
  }
}
