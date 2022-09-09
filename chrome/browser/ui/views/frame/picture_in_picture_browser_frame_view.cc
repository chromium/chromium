// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
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
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_shape.h"

namespace {

// TODO(https://crbug.com/1346734): Check whether any of the below should be
// based on platform constants instead.

constexpr int kWindowIconImageSize = 14;
constexpr int kBackToTabImageSize = 14;

// The height of the controls bar at the top of the window.
constexpr int kTopControlsHeight = 30;

constexpr int kWindowBorderThickness = 5;
constexpr int kResizeAreaCornerSize = 10;

// The window has a standard Chrome minimum size and does not have a maximum
// size.
constexpr gfx::Size kMinWindowSize(500, 500);

class BackToTabButton : public OverlayWindowImageButton {
 public:
  METADATA_HEADER(BackToTabButton);

  explicit BackToTabButton(PressedCallback callback)
      : OverlayWindowImageButton(std::move(callback)) {
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kBackToTabIcon, kColorPipWindowForeground, kBackToTabImageSize));

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
  return kTopControlsHeight;
}

int PictureInPictureBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect PictureInPictureBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return bounds();
}

int PictureInPictureBrowserFrameView::NonClientHitTest(
    const gfx::Point& point) {
  // Do nothing if the click is outside the window.
  if (!bounds().Contains(point))
    return HTNOWHERE;

  // Allow interacting with the buttons.
  if (GetLocationIconViewBounds().Contains(point) ||
      GetBackToTabControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point))
    return HTCLIENT;

  // Allow dragging and resizing the window.
  int window_component = GetHTComponentForFrame(
      point, gfx::Insets(kWindowBorderThickness), kResizeAreaCornerSize,
      kResizeAreaCornerSize, GetWidget()->widget_delegate()->CanResize());
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

void PictureInPictureBrowserFrameView::OnThemeChanged() {
  BrowserNonClientFrameView::OnThemeChanged();

  const auto* color_provider = GetColorProvider();
  window_background_view_->SetBackground(views::CreateSolidBackground(
      color_provider->GetColor(kColorPipWindowBackground)));
  controls_container_view_->SetBackground(views::CreateSolidBackground(
      SkColorSetA(color_provider->GetColor(kColorPipWindowControlsBackground),
                  SK_AlphaOPAQUE)));
}

void PictureInPictureBrowserFrameView::Layout() {
  controls_container_view_->SetBoundsRect(
      gfx::Rect(0, 0, width(), kTopControlsHeight));

  BrowserNonClientFrameView::Layout();
}

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
// PictureInPictureBrowserFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameView::GetLocationIconViewBounds() const {
  DCHECK(location_icon_view_);
  return location_icon_view_->GetMirroredBounds();
}

gfx::Rect PictureInPictureBrowserFrameView::GetBackToTabControlsBounds() const {
  DCHECK(back_to_tab_button_);
  return back_to_tab_button_->GetMirroredBounds();
}

gfx::Rect PictureInPictureBrowserFrameView::GetCloseControlsBounds() const {
  DCHECK(close_image_button_);
  return close_image_button_->GetMirroredBounds();
}

LocationIconView* PictureInPictureBrowserFrameView::GetLocationIconView() {
  return location_icon_view_;
}

BEGIN_METADATA(PictureInPictureBrowserFrameView, BrowserNonClientFrameView)
END_METADATA
