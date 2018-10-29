// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// The default time that the bubble should stay on the screen if it will close
// automatically.
constexpr base::TimeDelta kBubbleCloseDelayDefault =
    base::TimeDelta::FromMilliseconds(1500);

// A longer timeout used for how long the bubble should stay on the screen
// before it will close automatically after + or - buttons have been used.
constexpr base::TimeDelta kBubbleCloseDelayLong =
    base::TimeDelta::FromMilliseconds(5000);

// Creates an ImageButton using vector |icon|, sets a tooltip with |tooltip_id|.
// Returns the button.
std::unique_ptr<views::Button> CreateZoomButton(views::ButtonListener* listener,
                                                const gfx::VectorIcon& icon,
                                                int tooltip_id) {
  std::unique_ptr<views::ImageButton> button(
      views::CreateVectorImageButton(listener));
  views::SetImageFromVectorIcon(button.get(), icon);
  button->SetFocusForPlatform();
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_id));
  return std::move(button);
}

class ZoomValue : public views::Label {
 public:
  explicit ZoomValue(const content::WebContents* web_contents)
      : Label(base::string16(),
              views::style::CONTEXT_LABEL,
              views::style::STYLE_PRIMARY),
        max_width_(GetLabelMaxWidth(web_contents)) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  ~ZoomValue() override {}

  // views::Label:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(max_width_, GetHeightForWidth(max_width_));
  }

 private:
  int GetLabelMaxWidth(const content::WebContents* web_contents) const {
    const int border_width = border() ? border()->GetInsets().width() : 0;
    int max_w = 0;
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    DCHECK(zoom_controller);
    // Enumerate all zoom factors that can be used in PageZoom::Zoom.
    std::vector<double> zoom_factors =
        zoom::PageZoom::PresetZoomFactors(zoom_controller->GetZoomPercent());
    for (auto zoom : zoom_factors) {
      int w = gfx::GetStringWidth(
          base::FormatPercent(static_cast<int>(std::round(zoom * 100))),
          font_list());
      max_w = std::max(w, max_w);
    }
    return max_w + border_width;
  }

  const int max_width_;

  DISALLOW_COPY_AND_ASSIGN(ZoomValue);
};

bool IsBrowserFullscreen(Browser* browser) {
  DCHECK(browser->window() &&
         browser->exclusive_access_manager()->fullscreen_controller());
  return browser->window()->IsFullscreen();
}

PageActionIconContainerView* GetAnchorViewForBrowser(Browser* browser,
                                                     bool is_fullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!is_fullscreen ||
      browser_view->immersive_mode_controller()->IsRevealed()) {
    return browser_view->toolbar_button_provider()
        ->GetPageActionIconContainerView();
  }
  return nullptr;
}

PageActionIconContainerView* GetAnchorViewForBrowser(Browser* browser) {
  const bool is_fullscreen = IsBrowserFullscreen(browser);
  return GetAnchorViewForBrowser(browser, is_fullscreen);
}

ImmersiveModeController* GetImmersiveModeControllerForBrowser(
    Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->immersive_mode_controller();
}

void ParentToViewsBrowser(Browser* browser,
                          ZoomBubbleView* zoom_bubble,
                          views::View* anchor_view,
                          content::WebContents* web_contents) {
  // If the anchor view exists the zoom icon should be highlighed.
  if (anchor_view) {
    zoom_bubble->SetHighlightedButton(
        BrowserView::GetBrowserViewForBrowser(browser)
            ->toolbar_button_provider()
            ->GetPageActionIconContainerView()
            ->GetPageActionIconView(PageActionIconType::kZoom));
  } else {
    // If we do not have an anchor view, parent the bubble to the content area.
    zoom_bubble->set_parent_window(web_contents->GetNativeView());
  }

  views::BubbleDialogDelegateView::CreateBubble(zoom_bubble);
}

void ParentToBrowser(Browser* browser,
                     ZoomBubbleView* zoom_bubble,
                     views::View* anchor_view,
                     content::WebContents* web_contents) {
  ParentToViewsBrowser(browser, zoom_bubble, anchor_view, web_contents);
}

// Find the extension that initiated the zoom change, if any.
const extensions::ExtensionZoomRequestClient* GetExtensionZoomRequestClient(
    const content::WebContents* web_contents) {
  const zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  const zoom::ZoomRequestClient* client = zoom_controller->last_client();
  return static_cast<const extensions::ExtensionZoomRequestClient*>(client);
}

}  // namespace

// static
ZoomBubbleView* ZoomBubbleView::zoom_bubble_ = nullptr;

// static
void ZoomBubbleView::ShowBubble(content::WebContents* web_contents,
                                const gfx::Point& anchor_point,
                                DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  // |web_contents| could have been unloaded if a tab gets closed and a mouse
  // event arrives before the zoom icon gets hidden.
  if (!browser)
    return;

  if (RefreshBubbleIfShowing(web_contents))
    return;

  // If the bubble is already showing but in a different tab, the current
  // bubble must be closed and a new one created.
  CloseCurrentBubble();

  const bool is_fullscreen = IsBrowserFullscreen(browser);
  views::View* anchor_view = GetAnchorViewForBrowser(browser, is_fullscreen);
  ImmersiveModeController* immersive_mode_controller =
      GetImmersiveModeControllerForBrowser(browser);

  zoom_bubble_ = new ZoomBubbleView(anchor_view, anchor_point, web_contents,
                                    reason, immersive_mode_controller);

  const extensions::ExtensionZoomRequestClient* client =
      GetExtensionZoomRequestClient(web_contents);

  // If the zoom change was initiated by an extension, capture the relevent
  // information from it.
  if (client)
    zoom_bubble_->SetExtensionInfo(client->extension());

  ParentToBrowser(browser, zoom_bubble_, anchor_view, web_contents);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen)
    zoom_bubble_->AdjustForFullscreen(browser->window()->GetBounds());

  zoom_bubble_->ShowForReason(reason);
  zoom_bubble_->UpdateZoomIconVisibility();
}

// static
bool ZoomBubbleView::RefreshBubbleIfShowing(
    const content::WebContents* web_contents) {
  if (!CanRefresh(web_contents))
    return false;

  DCHECK_EQ(web_contents, zoom_bubble_->web_contents());
  zoom_bubble_->Refresh();

  return true;
}

// static
bool ZoomBubbleView::CanRefresh(const content::WebContents* web_contents) {
  // Can't refresh when there's not already a bubble for this tab.
  if (!zoom_bubble_ || (zoom_bubble_->web_contents() != web_contents))
    return false;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser ||
      (zoom_bubble_->GetAnchorView() != GetAnchorViewForBrowser(browser)))
    return false;

  const extensions::ExtensionZoomRequestClient* client =
      GetExtensionZoomRequestClient(web_contents);

  // Allow refreshes when the client won't create its own bubble; otherwise
  // the existing bubble would show the wrong zoom value.
  if (client && client->ShouldSuppressBubble())
    return true;

  // Allow refreshes when the existing bubble has the same attribution for
  // the zoom change, so only the label needs updating.
  return zoom_bubble_->extension_info_.id ==
         (client ? client->extension()->id() : std::string());
}

// static
void ZoomBubbleView::CloseCurrentBubble() {
  if (zoom_bubble_)
    zoom_bubble_->CloseBubble();
}

// static
ZoomBubbleView* ZoomBubbleView::GetZoomBubble() {
  return zoom_bubble_;
}

void ZoomBubbleView::Refresh() {
  UpdateZoomPercent();
  StartTimerIfNecessary();
}

ZoomBubbleView::ZoomBubbleView(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    DisplayReason reason,
    ImmersiveModeController* immersive_mode_controller)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      auto_close_duration_(kBubbleCloseDelayDefault),
      image_button_(nullptr),
      label_(nullptr),
      zoom_out_button_(nullptr),
      zoom_in_button_(nullptr),
      reset_button_(nullptr),
      auto_close_(reason == AUTOMATIC),
      ignore_close_bubble_(false),
      immersive_mode_controller_(immersive_mode_controller),
      session_id_(
          chrome::FindBrowserWithWebContents(web_contents)->session_id()) {
  set_notify_enter_exit_on_child(true);
  if (immersive_mode_controller_)
    immersive_mode_controller_->AddObserver(this);
  UseCompactMargins();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::ZOOM);
}

ZoomBubbleView::~ZoomBubbleView() {
  if (immersive_mode_controller_)
    immersive_mode_controller_->RemoveObserver(this);
}

base::string16 ZoomBubbleView::GetAccessibleWindowTitle() const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  PageActionIconContainerView* page_action_icon_container_view =
      GetAnchorViewForBrowser(browser);
  if (!page_action_icon_container_view)
    return base::string16();

  PageActionIconView* zoom_view =
      page_action_icon_container_view->GetPageActionIconView(
          PageActionIconType::kZoom);
  return zoom_view->GetTextForTooltipAndAccessibleName();
}

int ZoomBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void ZoomBubbleView::OnFocus() {
  LocationBarBubbleDelegateView::OnFocus();
  StopTimer();
}

void ZoomBubbleView::OnBlur() {
  LocationBarBubbleDelegateView::OnBlur();

  const views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager && Contains(focus_manager->GetFocusedView()))
    return;

  StartTimerIfNecessary();
}

void ZoomBubbleView::OnGestureEvent(ui::GestureEvent* event) {
  if (!zoom_bubble_ || !zoom_bubble_->auto_close_ ||
      event->type() != ui::ET_GESTURE_TAP) {
    return;
  }

  auto_close_ = false;
  StopTimer();
  event->SetHandled();
}

void ZoomBubbleView::OnKeyEvent(ui::KeyEvent* event) {
  if (!zoom_bubble_ || !zoom_bubble_->auto_close_)
    return;

  const views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager && Contains(focus_manager->GetFocusedView()))
    StopTimer();
  else
    StartTimerIfNecessary();
}

void ZoomBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  StopTimer();
}

void ZoomBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  StartTimerIfNecessary();
}

void ZoomBubbleView::Init() {
  // Set up the layout of the zoom bubble.
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int spacing =
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      provider->GetInsetsMetric(INSETS_TOAST) - margins(), spacing);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(std::move(box_layout));

  // Calculate child views margins in |this| client view.
  const int label_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL);
  const gfx::Insets label_vertical_margin(
      label_vertical_spacing - margins().top(), 0,
      label_vertical_spacing - margins().bottom(), 0);

  // Account for the apparent margins that vector buttons have around icons.
  const int control_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL);
  const gfx::Insets control_vertical_margin(
      control_vertical_spacing - margins().top(), 0,
      control_vertical_spacing - margins().bottom(), 0);
  const gfx::Insets vector_button_margin(
      control_vertical_margin -
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON));

  // If this zoom change was initiated by an extension, that extension will be
  // attributed by showing its icon in the zoom bubble.
  if (extension_info_.icon_image) {
    image_button_ = new views::ImageButton(this);
    image_button_->SetTooltipText(
        l10n_util::GetStringFUTF16(IDS_TOOLTIP_ZOOM_EXTENSION_ICON,
                                   base::UTF8ToUTF16(extension_info_.name)));
    image_button_->SetImage(views::Button::STATE_NORMAL,
                            &extension_info_.icon_image->image_skia());
    AddChildView(image_button_);
  }

  // Add zoom label with the new zoom percent.
  label_ = new ZoomValue(web_contents());
  UpdateZoomPercent();
  label_->SetProperty(views::kMarginsKey,
                      new gfx::Insets(label_vertical_margin));
  AddChildView(label_);

  // Add extra padding between the zoom percent label and the buttons.
  constexpr int kPercentLabelPadding = 64;
  auto* label_padding_view = new views::View();
  label_padding_view->SetPreferredSize(gfx::Size(
      kPercentLabelPadding - spacing * 2, label_->GetPreferredSize().height()));
  AddChildView(label_padding_view);

  // Add Zoom Out ("-") button.
  std::unique_ptr<views::Button> zoom_out_button =
      CreateZoomButton(this, kRemoveIcon, IDS_ACCNAME_ZOOM_MINUS2);
  zoom_out_button_ = zoom_out_button.get();
  zoom_out_button_->SetProperty(views::kMarginsKey,
                                new gfx::Insets(vector_button_margin));
  AddChildView(zoom_out_button.release());

  // Add Zoom In ("+") button.
  std::unique_ptr<views::Button> zoom_in_button =
      CreateZoomButton(this, kAddIcon, IDS_ACCNAME_ZOOM_PLUS2);
  zoom_in_button_ = zoom_in_button.get();
  zoom_in_button_->SetProperty(views::kMarginsKey,
                               new gfx::Insets(vector_button_margin));
  AddChildView(zoom_in_button.release());

  // Add "Reset" button.
  reset_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_ZOOM_SET_DEFAULT));
  reset_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM_SET_DEFAULT));
  AddChildView(reset_button_);

  StartTimerIfNecessary();
}

void ZoomBubbleView::WindowClosing() {
  // |zoom_bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  bool this_bubble = zoom_bubble_ == this;
  if (this_bubble)
    zoom_bubble_ = nullptr;

  UpdateZoomIconVisibility();
}

void ZoomBubbleView::CloseBubble() {
  if (ignore_close_bubble_)
    return;

  // Widget's Close() is async, but we don't want to use zoom_bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  zoom_bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

void ZoomBubbleView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  // No button presses in this dialog should cause the dialog to close,
  // including when the zoom level is set to 100% as a result of a button press.
  base::AutoReset<bool> auto_ignore_close_bubble(&ignore_close_bubble_, true);

  // Extend the timer to give a user more time after any button is pressed.
  auto_close_duration_ = kBubbleCloseDelayLong;
  StartTimerIfNecessary();

  if (sender == image_button_) {
    DCHECK(extension_info_.icon_image) << "Invalid button press.";
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    chrome::AddSelectedTabWithURL(
        browser, GURL(base::StringPrintf("chrome://extensions?id=%s",
                                         extension_info_.id.c_str())),
        ui::PAGE_TRANSITION_FROM_API);
  } else if (sender == zoom_out_button_) {
    zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_OUT);
  } else if (sender == zoom_in_button_) {
    zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_IN);
  } else if (sender == reset_button_) {
    zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_RESET);
  } else {
    NOTREACHED();
  }
}

void ZoomBubbleView::OnImmersiveRevealStarted() {
  CloseBubble();
}

void ZoomBubbleView::OnImmersiveModeControllerDestroyed() {
  immersive_mode_controller_ = nullptr;
}

void ZoomBubbleView::OnExtensionIconImageChanged(
    extensions::IconImage* /* image */) {
  image_button_->SetImage(views::Button::STATE_NORMAL,
                          &extension_info_.icon_image->image_skia());
  image_button_->SchedulePaint();
}

void ZoomBubbleView::SetExtensionInfo(const extensions::Extension* extension) {
  DCHECK(extension);
  extension_info_.id = extension->id();
  extension_info_.name = extension->name();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia& default_extension_icon_image =
      *rb.GetImageSkiaNamed(IDR_EXTENSIONS_FAVICON);
  int icon_size = gfx::kFaviconSize;

  // We give first preference to an icon from the extension's icon set that
  // matches the size of the default. But not all extensions will declare an
  // icon set, or may not have an icon of the default size (we don't want the
  // bubble to display, for example, a very large icon). In that case, if there
  // is a browser-action icon (size-19) this is an acceptable alternative.
  const ExtensionIconSet& icons = extensions::IconsInfo::GetIcons(extension);
  bool has_default_sized_icon =
      !icons.Get(gfx::kFaviconSize, ExtensionIconSet::MATCH_EXACTLY).empty();
  if (has_default_sized_icon) {
    extension_info_.icon_image.reset(new extensions::IconImage(
        web_contents()->GetBrowserContext(), extension, icons, icon_size,
        default_extension_icon_image, this));
    return;
  }

  const extensions::ActionInfo* browser_action =
      extensions::ActionInfo::GetBrowserActionInfo(extension);
  if (!browser_action || browser_action->default_icon.empty())
    return;

  icon_size = browser_action->default_icon.map().begin()->first;
  extension_info_.icon_image.reset(
      new extensions::IconImage(web_contents()->GetBrowserContext(), extension,
                                browser_action->default_icon, icon_size,
                                default_extension_icon_image, this));
}

void ZoomBubbleView::UpdateZoomPercent() {
  label_->SetText(base::FormatPercent(
      zoom::ZoomController::FromWebContents(web_contents())->GetZoomPercent()));
  label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

void ZoomBubbleView::UpdateZoomIconVisibility() {
  // Note that we can't rely on web_contents() here, as it may have been
  // destroyed by the time we get this call. Also note parent_window() (if set)
  // may also be destroyed: the call to WindowClosing() may be triggered by
  // parent window destruction tearing down its child windows.
  Browser* browser = chrome::FindBrowserWithID(session_id_);
  if (browser && browser->window() &&
      browser->window()->GetPageActionIconContainer()) {
    browser->window()->GetPageActionIconContainer()->UpdatePageActionIcon(
        PageActionIconType::kZoom);
  }
}

void ZoomBubbleView::StartTimerIfNecessary() {
  if (!auto_close_)
    return;

  auto_close_timer_.Start(FROM_HERE, auto_close_duration_, this,
                          &ZoomBubbleView::CloseBubble);
}

void ZoomBubbleView::StopTimer() {
  auto_close_timer_.Stop();
}

ZoomBubbleView::ZoomBubbleExtensionInfo::ZoomBubbleExtensionInfo() {}

ZoomBubbleView::ZoomBubbleExtensionInfo::~ZoomBubbleExtensionInfo() {}
