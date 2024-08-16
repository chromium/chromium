// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include <algorithm>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/caption_button_placeholder_container.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/views/controls/menu/menu_runner.h"
#endif

#if BUILDFLAG(IS_WIN)
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

  CaptionButtonBackgroundImageSource(
      const CaptionButtonBackgroundImageSource&) = delete;
  CaptionButtonBackgroundImageSource& operator=(
      const CaptionButtonBackgroundImageSource&) = delete;

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
};

bool HitTestCaptionButton(views::Button* button, const gfx::Point& point) {
  return button && button->GetVisible() &&
         button->GetMirroredBounds().Contains(point);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, public:

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

  if (browser_view->AppUsesWindowControlsOverlay()) {
    layout_->SetWindowControlsOverlayEnabled(
        browser_view->IsWindowControlsOverlayEnabled(), this);
  }

  if (browser_view->AppUsesBorderlessMode()) {
    layout_->SetBorderlessModeEnabled(browser_view->IsBorderlessModeEnabled(),
                                      this);
  }
  SetLayoutManager(std::unique_ptr<views::LayoutManager>(layout_));

  // Expose this view as a generic container as it contains/paints many things.
  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
}

OpaqueBrowserFrameView::~OpaqueBrowserFrameView() {}

void OpaqueBrowserFrameView::InitViews() {
  web_app::AppBrowserController* controller =
      browser_view()->browser()->app_controller();

  if (controller && controller->IsWindowControlsOverlayEnabled()) {
    caption_button_placeholder_container_ =
        AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
  }

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
    AddChildView(views::Builder<TabIconView>()
                     .CopyAddressTo(&window_icon_)
                     .SetModel(this)
                     .SetCallback(base::BindRepeating(
                         &OpaqueBrowserFrameView::WindowIconPressed,
                         base::Unretained(this)))
                     .SetID(VIEW_ID_WINDOW_ICON)
                     .Build());
  }

  // If this is a web app window, the window title will be part of the
  // BrowserView and thus we don't need to create another one here.
  if (!controller) {
    // The window title appears above the web app frame toolbar (if present),
    // which surrounds the title with minimal-ui buttons on the left,
    // and other controls (such as the app menu button) on the right.
    window_title_ = new views::Label(browser_view()->GetWindowTitle());
    window_title_->SetVisible(browser_view()->ShouldShowWindowTitle());
    window_title_->SetSubpixelRenderingEnabled(false);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->SetID(VIEW_ID_WINDOW_TITLE);
    AddChildView(window_title_.get());
  }

#if BUILDFLAG(IS_WIN)
  if (browser_view()->AppUsesWindowControlsOverlay())
    UpdateCaptionButtonToolTipsForWindowControlsOverlay();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return layout_->GetBoundsForTabStripRegion(tabstrip_minimum_size, width());
}

gfx::Rect OpaqueBrowserFrameView::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  return layout_->GetBoundsForWebAppFrameToolbar(toolbar_preferred_size);
}

void OpaqueBrowserFrameView::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  layout_->LayoutWebAppWindowTitle(available_space, window_title_label);
}

int OpaqueBrowserFrameView::GetTopInset(bool restored) const {
  return layout_->NonClientTopHeight(restored);
}

void OpaqueBrowserFrameView::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void OpaqueBrowserFrameView::WindowControlsOverlayEnabledChanged() {
  bool enabled = browser_view()->IsWindowControlsOverlayEnabled();
  if (enabled) {
    caption_button_placeholder_container_ =
        AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
    UpdateCaptionButtonPlaceholderContainerBackground();
  } else {
    RemoveChildViewT(caption_button_placeholder_container_.get());
    caption_button_placeholder_container_ = nullptr;
  }

#if BUILDFLAG(IS_WIN)
  UpdateCaptionButtonToolTipsForWindowControlsOverlay();
#endif

  layout_->SetWindowControlsOverlayEnabled(enabled, this);
  InvalidateLayout();
}

gfx::Size OpaqueBrowserFrameView::GetMinimumSize() const {
  return layout_->GetMinimumSize(this);
}

void OpaqueBrowserFrameView::PaintAsActiveChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::PaintAsActiveChanged();
}

void OpaqueBrowserFrameView::OnThemeChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::OnThemeChanged();
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
    gfx::Rect sysmenu_rect(GetIconBounds());
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

  // BrowserView covers the frame view when Window Controls Overlay is enabled.
  // The native window that encompasses Web Contents gets the mouse events meant
  // for the caption buttons, so returning HTClient allows these buttons to be
  // highlighted on hover.
  if (browser_view()->IsWindowControlsOverlayEnabled() &&
      (HitTestCaptionButton(minimize_button_, point) ||
       HitTestCaptionButton(maximize_button_, point) ||
       HitTestCaptionButton(restore_button_, point) ||
       HitTestCaptionButton(close_button_, point)))
    return HTCLIENT;

  // Then see if the point is within any of the window controls.
  if (HitTestCaptionButton(close_button_, point))
    return HTCLOSE;
  if (HitTestCaptionButton(restore_button_, point))
    return HTMAXBUTTON;
  if (HitTestCaptionButton(maximize_button_, point))
    return HTMAXBUTTON;
  if (HitTestCaptionButton(minimize_button_, point))
    return HTMINBUTTON;

  if (browser_view()->IsWindowControlsOverlayEnabled() &&
      caption_button_placeholder_container_ &&
      caption_button_placeholder_container_->GetMirroredBounds().Contains(
          point)) {
    return HTCAPTION;
  }

  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate) {
    LOG(WARNING) << "delegate is null, returning safe default.";
    return HTCAPTION;
  }

  // In the window corners, the resize areas don't actually expand bigger, but
  // the 16 px at the end of each edge triggers diagonal resizing.
  constexpr int kResizeAreaCornerSize = 16;
  auto resize_border = FrameBorderInsets(false);
  if (base::i18n::IsRTL()) {
    resize_border =
        gfx::Insets::TLBR(resize_border.top(), resize_border.right(),
                          resize_border.bottom(), resize_border.left());
  }
  // The top resize border has extra thickness.
  resize_border.set_top(FrameTopBorderThickness(false));
  int window_component =
      GetHTComponentForFrame(point, resize_border, kResizeAreaCornerSize,
                             kResizeAreaCornerSize, delegate->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void OpaqueBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                           SkPath* window_mask) {
  DCHECK(window_mask);

  if (IsFrameCondensed())
    return;

  views::GetDefaultWindowMask(size, window_mask);
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
    DeprecatedLayoutImmediately();
    if (window_title_) {
      window_title_->SchedulePaint();
    }
  }
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

ui::ImageModel OpaqueBrowserFrameView::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate) {
    LOG(WARNING) << "delegate is null, returning safe default.";
    return ui::ImageModel();
  }
  return delegate->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, OpaqueBrowserFrameViewLayoutDelegate implementation:

bool OpaqueBrowserFrameView::ShouldShowWindowIcon() const {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return GetShowWindowTitleBar() && delegate &&
         delegate->ShouldShowWindowIcon();
}

bool OpaqueBrowserFrameView::ShouldShowWindowTitle() const {
  // |delegate| may be null if called from callback of InputMethodChanged while
  // a window is being destroyed.
  // See more discussion at http://crosbug.com/8958
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return GetShowWindowTitleBar() && delegate &&
         delegate->ShouldShowWindowTitle();
}

std::u16string OpaqueBrowserFrameView::GetWindowTitle() const {
  return frame()->widget_delegate()->GetWindowTitle();
}

int OpaqueBrowserFrameView::GetIconSize() const {
#if BUILDFLAG(IS_WIN)
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
  return GetShowWindowTitleBar();
}

bool OpaqueBrowserFrameView::IsRegularOrGuestSession() const {
  return browser_view()->GetRegularOrGuestSession();
}

bool OpaqueBrowserFrameView::CanMaximize() const {
  return browser_view()->CanMaximize();
}

bool OpaqueBrowserFrameView::CanMinimize() const {
  return browser_view()->CanMinimize();
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
  return browser_view()->GetTabStripVisible();
}

bool OpaqueBrowserFrameView::GetBorderlessModeEnabled() const {
  return browser_view()->IsBorderlessModeEnabled();
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
  int top_height = layout_->NonClientTopHeight(false);
  if (browser_view()->ShouldDrawTabStrip()) {
    top_height =
        std::max(top_height,
                 GetBoundsForTabStripRegion(GetTabstripMinimumSize()).bottom() -
                     GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
  }
    gfx::Rect web_app_toolbar_bounds = GetBoundsForWebAppFrameToolbar(
        browser_view()->GetWebAppFrameToolbarPreferredSize());
    if (!web_app_toolbar_bounds.IsEmpty()) {
      top_height = std::max(top_height, web_app_toolbar_bounds.bottom());
    }
  return top_height;
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
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return FrameButtonStyle::kMdButton;
#else
  return FrameButtonStyle::kImageButton;
#endif
}

void OpaqueBrowserFrameView::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {
  content::WebContents* web_contents = browser_view()->GetActiveWebContents();
  if (web_contents) {
    web_contents->UpdateWindowControlsOverlay(bounding_rect);
  }
}

bool OpaqueBrowserFrameView::ShouldDrawRestoredFrameShadow() const {
  return false;
}

#if BUILDFLAG(IS_LINUX)
bool OpaqueBrowserFrameView::IsTiled() const {
  return frame()->tiled();
}
#endif

int OpaqueBrowserFrameView::WebAppButtonHeight() const {
  return browser_view()->GetWebAppFrameToolbarPreferredSize().height();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, protected:

// views::View:
void OpaqueBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT0("views.frame", "OpaqueBrowserFrameView::OnPaint");
  if (frame()->IsFullscreen())
    return;  // Nothing is visible, so don't bother to paint.

  const bool active = ShouldPaintAsActive();
  SkColor frame_color = GetFrameColor(BrowserFrameActiveState::kUseCurrent);
  if (window_title_) {
    window_title_->SetEnabledColor(
        GetCaptionColor(BrowserFrameActiveState::kUseCurrent));
    window_title_->SetBackgroundColor(frame_color);
  }
  frame_background_->set_frame_color(frame_color);
  frame_background_->set_use_custom_frame(frame()->UseCustomFrame());
  frame_background_->set_is_active(active);
  frame_background_->set_theme_image(GetFrameImage());
  frame_background_->set_theme_image_inset(
      browser_view()->GetThemeOffsetFromBrowserView());
  frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
  frame_background_->set_top_area_height(GetTopAreaHeight());

  if (GetFrameButtonStyle() == FrameButtonStyle::kMdButton) {
    for (views::Button* button :
         {minimize_button_, maximize_button_, restore_button_, close_button_}) {
      DCHECK_EQ(std::string(views::FrameCaptionButton::kViewClassName),
                button->GetClassName());
      views::FrameCaptionButton* frame_caption_button =
          static_cast<views::FrameCaptionButton*>(button);
      frame_caption_button->SetPaintAsActive(active);
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
  views::FrameCaptionButton* button = new views::FrameCaptionButton(
      views::Button::PressedCallback(), icon_type, ht_component);
  button->SetImage(button->GetIcon(), views::FrameCaptionButton::Animate::kNo,
                   icon_image);
  return button;
}

views::Button* OpaqueBrowserFrameView::CreateImageButton(int normal_image_id,
                                                         int hot_image_id,
                                                         int pushed_image_id,
                                                         int mask_image_id,
                                                         ViewID view_id) {
  views::ImageButton* button =
      new views::ImageButton(views::Button::PressedCallback());
  const ui::ThemeProvider* tp = frame()->GetThemeProvider();
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(*tp->GetImageSkiaNamed(normal_image_id)));
  button->SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromImageSkia(*tp->GetImageSkiaNamed(hot_image_id)));
  button->SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromImageSkia(*tp->GetImageSkiaNamed(pushed_image_id)));
  button->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  if (browser_view()->GetIsNormalType()) {
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
        frame()->GetColorProvider()->GetColor(kColorCaptionButtonBackground),
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
  button->SetCallback(std::move(callback));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(accessibility_string_id));
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

gfx::Insets OpaqueBrowserFrameView::FrameBorderInsets(bool restored) const {
  return layout_->FrameBorderInsets(restored);
}

int OpaqueBrowserFrameView::FrameTopBorderThickness(bool restored) const {
  return layout_->FrameTopBorderThickness(restored);
}

gfx::Rect OpaqueBrowserFrameView::GetIconBounds() const {
  return layout_->IconBounds();
}

void OpaqueBrowserFrameView::WindowIconPressed() {
#if BUILDFLAG(IS_LINUX)
  // Chrome OS doesn't show the window icon, and Windows handles this on its own
  // due to the hit test being HTSYSMENU.
  menu_runner_ = std::make_unique<views::MenuRunner>(
      frame()->GetSystemMenuModel(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(
      browser_view()->GetWidget(), window_icon_->button_controller(),
      window_icon_->GetBoundsInScreen(), views::MenuAnchorPosition::kTopLeft,
      ui::MENU_SOURCE_MOUSE);
#endif
}

bool OpaqueBrowserFrameView::GetShowWindowTitleBar() const {
  // Do not show the custom title bar if the system title bar option is enabled.
  if (!frame()->UseCustomFrame())
    return false;

  if (frame()->IsFullscreen())
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
  const bool tabstrip_visible = browser_view()->ShouldDrawTabStrip();
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
      GetColorProvider()->GetColor(kColorLocationBarBorderOpaque);
  if (!tabstrip_visible && IsToolbarVisible()) {
    gfx::Rect side(client_bounds.x() - kClientEdgeThickness, y,
                   kClientEdgeThickness, toolbar_bounds.height());
    canvas->FillRect(side, location_bar_border_color);
    side.Offset(client_bounds.width() + kClientEdgeThickness, 0);
    canvas->FillRect(side, location_bar_border_color);
  }
}

void OpaqueBrowserFrameView::
    UpdateCaptionButtonPlaceholderContainerBackground() {
  if (caption_button_placeholder_container_) {
    caption_button_placeholder_container_->SetBackground(
        views::CreateSolidBackground(
            GetFrameColor(BrowserFrameActiveState::kUseCurrent)));
  }
}

#if BUILDFLAG(IS_WIN)
void OpaqueBrowserFrameView::
    UpdateCaptionButtonToolTipsForWindowControlsOverlay() {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    minimize_button_->SetTooltipText(
        minimize_button_->GetViewAccessibility().GetCachedName());
    maximize_button_->SetTooltipText(
        maximize_button_->GetViewAccessibility().GetCachedName());
    restore_button_->SetTooltipText(
        restore_button_->GetViewAccessibility().GetCachedName());
    close_button_->SetTooltipText(
        close_button_->GetViewAccessibility().GetCachedName());
  } else {
    minimize_button_->SetTooltipText(u"");
    maximize_button_->SetTooltipText(u"");
    restore_button_->SetTooltipText(u"");
    close_button_->SetTooltipText(u"");
  }
}
#endif

BEGIN_METADATA(OpaqueBrowserFrameView)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, IconBounds)
ADD_READONLY_PROPERTY_METADATA(bool, ShowWindowTitleBar)
END_METADATA
