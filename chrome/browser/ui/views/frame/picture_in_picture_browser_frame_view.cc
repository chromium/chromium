// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_shape.h"

#if !BUILDFLAG(IS_MAC)
// Mac does not use Aura
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#endif

namespace {

// TODO(https://crbug.com/1346734): Check whether any of the below should be
// based on platform constants instead.

constexpr int kWindowIconImageSize = 14;
constexpr int kBackToTabImageSize = 14;

// The height of the controls bar at the top of the window.
constexpr int kTopControlsHeight = 30;

constexpr int kWindowBorderThickness = 10;
constexpr int kResizeAreaCornerSize = 16;

// The window has a smaller minimum size than normal Chrome windows.
constexpr gfx::Size kMinWindowSize(300, 300);

class BackToTabButton : public OverlayWindowImageButton {
 public:
  METADATA_HEADER(BackToTabButton);

  explicit BackToTabButton(PressedCallback callback)
      : OverlayWindowImageButton(std::move(callback)) {
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(vector_icons::kBackToTabIcon,
                                                 kColorPipWindowForeground,
                                                 kBackToTabImageSize));

    const std::u16string back_to_tab_button_label = l10n_util::GetStringUTF16(
        IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT);
    SetTooltipText(back_to_tab_button_label);
  }
  BackToTabButton(const BackToTabButton&) = delete;
  BackToTabButton& operator=(const BackToTabButton&) = delete;
  ~BackToTabButton() override = default;
};

BEGIN_METADATA(BackToTabButton, OverlayWindowImageButton)
END_METADATA

}  // namespace

PictureInPictureBrowserFrameView::PictureInPictureBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      this, content::kMaxURLDisplayChars);

  // Creates a window background with solid color.
  // TODO(https://crbug.com/1346734): Need to figure out how to make this
  // background color not overlap pip content. AddChildView() would cause it to
  // overlap while now it never shows.
  window_background_view_ = browser_view->contents_web_view()->AddChildViewAt(
      std::make_unique<views::View>(), 0);

  // Creates a view that will hold all the control views.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&controls_container_view_)
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .Build());

  // Creates the window icon.
  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_ = controls_container_view_->AddChildView(
      std::make_unique<LocationIconView>(font_list, this, this));

  // Creates the window title.
  controls_container_view_->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&window_title_)
          .SetText(location_bar_model_->GetURLForDisplay())
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());
  controls_container_view_->SetFlexForView(window_title_, 1);

  // Creates the content setting models. Currently we only support geo location
  // and camera and microphone settings.
  constexpr ContentSettingImageModel::ImageType kContentSettingImageOrder[] = {
      ContentSettingImageModel::ImageType::GEOLOCATION,
      ContentSettingImageModel::ImageType::MEDIASTREAM};
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  for (auto type : kContentSettingImageOrder)
    models.push_back(ContentSettingImageModel::CreateForContentType(type));

  // Creates the content setting views.
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this, this, font_list);
    content_setting_views_.push_back(
        controls_container_view_->AddChildView(std::move(image_view)));
  }

  // Creates the back to tab button.
  back_to_tab_button_ = controls_container_view_->AddChildView(
      std::make_unique<BackToTabButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            // TODO(https://crbug.com/1346734): Focus the original tab too.
            PictureInPictureWindowManager::GetInstance()
                ->ExitPictureInPicture();
          },
          base::Unretained(this))));

  // Creates the close button.
  close_image_button_ = controls_container_view_->AddChildView(
      std::make_unique<CloseImageButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            PictureInPictureWindowManager::GetInstance()
                ->ExitPictureInPicture();
          },
          base::Unretained(this))));
}

PictureInPictureBrowserFrameView::~PictureInPictureBrowserFrameView() = default;

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return gfx::Rect();
}

int PictureInPictureBrowserFrameView::GetTopInset(bool restored) const {
  return GetTopAreaHeight();
}

int PictureInPictureBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForClientView() const {
  auto border_thickness = FrameBorderInsets();
  int top_height = GetTopAreaHeight();
  return gfx::Rect(border_thickness.left(), top_height,
                   width() - border_thickness.width(),
                   height() - top_height - border_thickness.bottom());
}

gfx::Rect PictureInPictureBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  auto border_thickness = FrameBorderInsets();
  int top_height = GetTopAreaHeight();
  return gfx::Rect(
      client_bounds.x() - border_thickness.left(),
      client_bounds.y() - top_height,
      client_bounds.width() + border_thickness.width(),
      client_bounds.height() + top_height + border_thickness.bottom());
}

int PictureInPictureBrowserFrameView::NonClientHitTest(
    const gfx::Point& point) {
  // Do nothing if the click is outside the window.
  if (!GetLocalBounds().Contains(point))
    return HTNOWHERE;

  // Allow interacting with the buttons.
  if (GetLocationIconViewBounds().Contains(point) ||
      GetBackToTabControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point))
    return HTCLIENT;

  for (size_t i = 0; i < content_setting_views_.size(); i++) {
    if (GetContentSettingViewBounds(i).Contains(point))
      return HTCLIENT;
  }

  // Allow dragging and resizing the window.
  int window_component = GetHTComponentForFrame(
      point, FrameBorderInsets(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      GetWidget()->widget_delegate()->CanResize());
  if (window_component != HTNOWHERE)
    return window_component;

  // Allow interacting with the web contents.
  int frame_component = frame()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  return HTCAPTION;
}

void PictureInPictureBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                                     SkPath* window_mask) {
  DCHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

void PictureInPictureBrowserFrameView::UpdateWindowIcon() {
  // This will be called after WebContents in PictureInPictureWindowManager is
  // set, so that we can update the icon and title based on WebContents.
  location_icon_view_->Update(/*suppress_animations=*/false);
  window_title_->SetText(location_bar_model_->GetURLForDisplay());
}

gfx::Size PictureInPictureBrowserFrameView::GetMinimumSize() const {
  return kMinWindowSize;
}

gfx::Size PictureInPictureBrowserFrameView::GetMaximumSize() const {
  if (!GetWidget() || !GetWidget()->GetNativeWindow())
    return gfx::Size();

  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  return gfx::ScaleToRoundedSize(display.size(), 0.8);
}

void PictureInPictureBrowserFrameView::OnThemeChanged() {
  const auto* color_provider = GetColorProvider();
  window_background_view_->SetBackground(views::CreateSolidBackground(
      color_provider->GetColor(kColorPipWindowBackground)));
  window_title_->SetEnabledColor(
      color_provider->GetColor(kColorPipWindowForeground));
  for (ContentSettingImageView* view : content_setting_views_)
    view->SetIconColor(color_provider->GetColor(kColorOmniboxResultsIcon));

#if BUILDFLAG(IS_LINUX)
  // If the top bar background is already drawn by window_frame_provider_, skip
  // drawing it again below.
  if (window_frame_provider_) {
    BrowserNonClientFrameView::OnThemeChanged();
    return;
  }
#endif
  controls_container_view_->SetBackground(views::CreateSolidBackground(
      SkColorSetA(color_provider->GetColor(kColorPipWindowControlsBackground),
                  SK_AlphaOPAQUE)));
  BrowserNonClientFrameView::OnThemeChanged();
}

void PictureInPictureBrowserFrameView::Layout() {
  auto border_thickness = FrameBorderInsets();
  controls_container_view_->SetBoundsRect(
      gfx::Rect(border_thickness.left(), border_thickness.top(),
                width() - border_thickness.width(), kTopControlsHeight));

  BrowserNonClientFrameView::Layout();
}

void PictureInPictureBrowserFrameView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());

#if !BUILDFLAG(IS_MAC)
  // For non-Mac platforms that use Aura, add a pre target handler to receive
  // events before the Widget so that we can override event handlers to update
  // the top bar view.
  GetWidget()->GetNativeWindow()->AddPreTargetHandler(this);
#endif

  BrowserNonClientFrameView::AddedToWidget();
}

#if BUILDFLAG(IS_LINUX)
gfx::Insets PictureInPictureBrowserFrameView::MirroredFrameBorderInsets()
    const {
  auto border = FrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets PictureInPictureBrowserFrameView::GetInputInsets() const {
  return gfx::Insets(ShouldDrawFrameShadow() ? -kWindowBorderThickness : 0);
}

SkRRect PictureInPictureBrowserFrameView::GetRestoredClipRegion() const {
  gfx::RectF bounds_dip(GetLocalBounds());
  if (ShouldDrawFrameShadow()) {
    gfx::InsetsF border(MirroredFrameBorderInsets());
    bounds_dip.Inset(border);
  }

  float radius_dip = 0;
  if (window_frame_provider_) {
    radius_dip = window_frame_provider_->GetTopCornerRadiusDip();
  }
  SkVector radii[4]{{radius_dip, radius_dip}, {radius_dip, radius_dip}, {}, {}};
  SkRRect clip;
  clip.setRectRadii(gfx::RectFToSkRect(bounds_dip), radii);
  return clip;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// ChromeLocationBarModelDelegate implementations:

content::WebContents* PictureInPictureBrowserFrameView::GetActiveWebContents()
    const {
  return PictureInPictureWindowManager::GetInstance()->GetWebContents();
}

bool PictureInPictureBrowserFrameView::GetURL(GURL* url) const {
  DCHECK(url);
  if (GetActiveWebContents()) {
    *url = GetActiveWebContents()->GetLastCommittedURL();
    return true;
  }
  return false;
}

bool PictureInPictureBrowserFrameView::ShouldTrimDisplayUrlAfterHostName()
    const {
  // We need to set the window title URL to be eTLD+1.
  return true;
}

bool PictureInPictureBrowserFrameView::ShouldDisplayURL() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// LocationIconView::Delegate implementations:

content::WebContents* PictureInPictureBrowserFrameView::GetWebContents() {
  return PictureInPictureWindowManager::GetInstance()->GetWebContents();
}

bool PictureInPictureBrowserFrameView::IsEditingOrEmpty() const {
  return false;
}

SkColor PictureInPictureBrowserFrameView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  return GetColorProvider()->GetColor(kColorOmniboxSecurityChipSecure);
}

bool PictureInPictureBrowserFrameView::ShowPageInfoDialog() {
  content::WebContents* contents = GetWebContents();
  if (!contents)
    return false;

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          location_icon_view_, gfx::Rect(), GetWidget()->GetNativeWindow(),
          contents, contents->GetLastCommittedURL(),
          /*initialized_callback=*/base::DoNothing(),
          /*closing_callback=*/base::DoNothing());
  bubble->SetHighlightedButton(location_icon_view_);
  bubble->GetWidget()->Show();
  return true;
}

LocationBarModel* PictureInPictureBrowserFrameView::GetLocationBarModel()
    const {
  return location_bar_model_.get();
}

ui::ImageModel PictureInPictureBrowserFrameView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  return ui::ImageModel::FromVectorIcon(location_bar_model_->GetVectorIcon(),
                                        kColorOmniboxSecurityChipSecure,
                                        kWindowIconImageSize);
}

///////////////////////////////////////////////////////////////////////////////
// IconLabelBubbleView::Delegate implementations:

SkColor
PictureInPictureBrowserFrameView::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return GetColorProvider()->GetColor(kColorOmniboxText);
}

SkColor PictureInPictureBrowserFrameView::GetIconLabelBubbleBackgroundColor()
    const {
  return GetColorProvider()->GetColor(kColorLocationBarBackground);
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingImageView::Delegate implementations:

bool PictureInPictureBrowserFrameView::ShouldHideContentSettingImage() {
  return false;
}

content::WebContents*
PictureInPictureBrowserFrameView::GetContentSettingWebContents() {
  // Use the opener web contents for content settings since it has full info
  // such as last committed URL, etc. that are called to be used.
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
PictureInPictureBrowserFrameView::GetContentSettingBubbleModelDelegate() {
  // Use the opener browser delegate to open any new tab.
  Browser* browser = chrome::FindBrowserWithWebContents(GetWebContents());
  return browser->content_setting_bubble_model_delegate();
}

///////////////////////////////////////////////////////////////////////////////
// GeolocationManager::PermissionObserver implementations:
void PictureInPictureBrowserFrameView::OnSystemPermissionUpdated(
    device::LocationSystemPermissionStatus new_status) {
  // Update icons if the macOS location permission is updated.
  UpdateContentSettingsIcons();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver implementations:
void PictureInPictureBrowserFrameView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  // The window may become inactive when a popup modal shows, so we need to
  // check if the mouse is still inside the window.
  if (!active && mouse_inside_window_)
    active = true;
  UpdateTopBarView(active);
}

void PictureInPictureBrowserFrameView::OnWidgetDestroying(
    views::Widget* widget) {
#if !BUILDFLAG(IS_MAC)
  widget->GetNativeWindow()->RemovePreTargetHandler(this);
#endif

  widget_observation_.Reset();
}

///////////////////////////////////////////////////////////////////////////////
// ui::EventHandler implementations:
void PictureInPictureBrowserFrameView::OnKeyEvent(ui::KeyEvent* event) {
  // Highlight when a user uses a keyboard to interact on the window.
  UpdateTopBarView(true);
}

void PictureInPictureBrowserFrameView::OnMouseEvent(ui::MouseEvent* event) {
  // TODO(https://crbug.com/1346734): This does not work on Mac since Mac does
  // not use Aura, so we need to find another way for Mac.
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      if (!mouse_inside_window_) {
        mouse_inside_window_ = true;
        UpdateTopBarView(true);
      }
      break;

    case ui::ET_MOUSE_EXITED: {
      // This can be triggered even when the mouse is still over the window such
      // as on the content settings popup modal, so we need to check the bounds.
      if (!GetLocalBounds().Contains(event->location())) {
        mouse_inside_window_ = false;
        UpdateTopBarView(false);
      }
      break;
    }

    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:
void PictureInPictureBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
#if BUILDFLAG(IS_LINUX)
  if (window_frame_provider_) {
    // Draw the PiP window frame borders and shadows, including the top bar
    // background.
    window_frame_provider_->PaintWindowFrame(
        canvas, GetLocalBounds(), GetTopAreaHeight(), ShouldPaintAsActive(),
        frame()->tiled_edges());
  }
#endif
  BrowserNonClientFrameView::OnPaint(canvas);
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView implementations:
gfx::Rect PictureInPictureBrowserFrameView::ConvertControlViewBounds(
    views::View* control_view) const {
  gfx::RectF bounds(control_view->GetMirroredBounds());
  views::View::ConvertRectToTarget(controls_container_view_, this, &bounds);
  return gfx::ToEnclosingRect(bounds);
}

gfx::Rect PictureInPictureBrowserFrameView::GetLocationIconViewBounds() const {
  DCHECK(location_icon_view_);
  return ConvertControlViewBounds(location_icon_view_);
}

gfx::Rect PictureInPictureBrowserFrameView::GetContentSettingViewBounds(
    size_t index) const {
  DCHECK(index < content_setting_views_.size());
  return ConvertControlViewBounds(content_setting_views_[index]);
}

gfx::Rect PictureInPictureBrowserFrameView::GetBackToTabControlsBounds() const {
  DCHECK(back_to_tab_button_);
  return ConvertControlViewBounds(back_to_tab_button_);
}

gfx::Rect PictureInPictureBrowserFrameView::GetCloseControlsBounds() const {
  DCHECK(close_image_button_);
  return ConvertControlViewBounds(close_image_button_);
}

LocationIconView* PictureInPictureBrowserFrameView::GetLocationIconView() {
  return location_icon_view_;
}

void PictureInPictureBrowserFrameView::UpdateContentSettingsIcons() {
  for (auto* view : content_setting_views_) {
    view->Update();
  }
}

void PictureInPictureBrowserFrameView::UpdateTopBarView(bool render_active) {
  back_to_tab_button_->SetVisible(render_active);
  close_image_button_->SetVisible(render_active);

  SkColor color;
  if (render_active) {
    color = GetColorProvider()->GetColor(kColorPipWindowForeground);
  } else {
    color = GetColorProvider()->GetColor(kColorOmniboxResultsIcon);
  }
  window_title_->SetEnabledColor(color);
  for (ContentSettingImageView* view : content_setting_views_)
    view->SetIconColor(color);
}

gfx::Insets PictureInPictureBrowserFrameView::FrameBorderInsets() const {
#if BUILDFLAG(IS_LINUX)
  if (window_frame_provider_) {
    const auto insets = window_frame_provider_->GetFrameThicknessDip();
    const auto tiled_edges = frame()->tiled_edges();

    // If edges of the window are tiled and snapped to the edges of the desktop,
    // window_frame_provider_ will skip drawing.
    return gfx::Insets::TLBR(tiled_edges.top ? 0 : insets.top(),
                             tiled_edges.left ? 0 : insets.left(),
                             tiled_edges.bottom ? 0 : insets.bottom(),
                             tiled_edges.right ? 0 : insets.right());
  }
#endif
  return gfx::Insets(kWindowBorderThickness);
}

int PictureInPictureBrowserFrameView::GetTopAreaHeight() const {
  return FrameBorderInsets().top() + kTopControlsHeight;
}

#if BUILDFLAG(IS_LINUX)
void PictureInPictureBrowserFrameView::SetWindowFrameProvider(
    ui::WindowFrameProvider* window_frame_provider) {
  window_frame_provider_ = window_frame_provider;
}

bool PictureInPictureBrowserFrameView::ShouldDrawFrameShadow() const {
  return static_cast<DesktopBrowserFrameAuraLinux*>(
             frame()->native_browser_frame())
      ->ShouldDrawRestoredFrameShadow();
}
#endif

BEGIN_METADATA(PictureInPictureBrowserFrameView, BrowserNonClientFrameView)
END_METADATA
