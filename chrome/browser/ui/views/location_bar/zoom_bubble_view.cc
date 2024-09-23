// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include <cmath>
#include <memory>

#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// The default time that the bubble should stay on the screen if it will close
// automatically.
constexpr base::TimeDelta kBubbleCloseDelayDefault = base::Milliseconds(1500);

// A longer timeout used for how long the bubble should stay on the screen
// before it will close automatically after + or - buttons have been used.
constexpr base::TimeDelta kBubbleCloseDelayLong = base::Milliseconds(5000);

class ZoomButtonHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  ZoomButtonHighlightPathGenerator() = default;

  SkPath GetHighlightPath(const views::View* view) override {
    constexpr int kCircleRadiusDp = 24 / 2;
    const gfx::Point center = view->GetLocalBounds().CenterPoint();
    return SkPath().addCircle(center.x(), center.y(), kCircleRadiusDp);
  }
};

std::unique_ptr<views::ImageButton> CreateZoomButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int tooltip_id) {
  auto zoom_button =
      views::CreateVectorImageButtonWithNativeTheme(std::move(callback), icon);
  zoom_button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_id));
  views::HighlightPathGenerator::Install(
      zoom_button.get(), std::make_unique<ZoomButtonHighlightPathGenerator>());
  return zoom_button;
}

class ZoomValue : public views::Label {
  METADATA_HEADER(ZoomValue, views::Label)

 public:
  explicit ZoomValue(const content::WebContents* web_contents)
      : Label(std::u16string(),
              views::style::CONTEXT_LABEL,
              views::style::STYLE_PRIMARY),
        max_width_(GetLabelMaxWidth(web_contents)) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  ZoomValue(const ZoomValue&) = delete;
  ZoomValue& operator=(const ZoomValue&) = delete;
  ~ZoomValue() override = default;

  // views::Label:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size =
        views::Label::CalculatePreferredSize(views::SizeBounds(max_width_, {}));
    // When the initial value of the text width is small(eg: 80%), the
    // `ZoomBubbleView` will be smaller. Then after we use a larger value(eg:
    // 200%), the text will not be fully displayed. It needs to be set to the
    // maximum value to ensure that the size of `ZoomBubbleView` is fixed.
    size.set_width(max_width_);
    return size;
  }

 private:
  int GetLabelMaxWidth(const content::WebContents* web_contents) const {
    const int border_width = GetInsets().width();
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
};

BEGIN_METADATA(ZoomValue)
END_METADATA

bool IsBrowserFullscreen(Browser* browser) {
  DCHECK(browser->window() &&
         browser->exclusive_access_manager()->fullscreen_controller());
  return browser->window()->IsFullscreen();
}

views::View* GetAnchorViewForBrowser(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser)) {
    return nullptr;
  }
#endif
  if (!IsBrowserFullscreen(browser) || browser_view->IsToolbarVisible() ||
      browser_view->immersive_mode_controller()->IsRevealed()) {
    return browser_view->toolbar_button_provider()->GetAnchorView(
        PageActionIconType::kZoom);
  }
  return nullptr;
}

ImmersiveModeController* GetImmersiveModeControllerForBrowser(
    Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->immersive_mode_controller();
}

void ParentToBrowser(Browser* browser,
                     ZoomBubbleView* zoom_bubble,
                     views::View* anchor_view,
                     content::WebContents* web_contents) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  zoom_bubble->SetHighlightedButton(
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kZoom));

  // If we don't anchor to anything the BrowserView is our parent. This happens
  // in fullscreen cases.
  zoom_bubble->set_parent_window(
      zoom_bubble->anchor_widget()
          ? nullptr
          : browser_view->GetWidget()->GetNativeView());

  views::BubbleDialogDelegateView::CreateBubble(zoom_bubble);
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
                                DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  // |web_contents| could have been unloaded if a tab gets closed and a mouse
  // event arrives before the zoom icon gets hidden.
  if (!browser)
    return;

  if (RefreshBubbleIfShowing(web_contents))
    return;

  // If the bubble is already showing but in a different tab, the current
  // bubble must be closed and a new one created.
  CloseCurrentBubble();

  views::View* anchor_view = GetAnchorViewForBrowser(browser);
  ImmersiveModeController* immersive_mode_controller =
      GetImmersiveModeControllerForBrowser(browser);

  zoom_bubble_ = new ZoomBubbleView(anchor_view, web_contents, reason,
                                    immersive_mode_controller);

  const extensions::ExtensionZoomRequestClient* client =
      GetExtensionZoomRequestClient(web_contents);

  // If the zoom change was initiated by an extension, capture the relevent
  // information from it.
  if (client)
    zoom_bubble_->SetExtensionInfo(client->extension());

  ParentToBrowser(browser, zoom_bubble_, anchor_view, web_contents);

  if (!anchor_view && IsBrowserFullscreen(browser))
    zoom_bubble_->AdjustForFullscreen(browser->window()->GetBounds());

  // Do not announce hotkey for refocusing inactive Zoom bubble as it
  // disappears after a short timeout.
  zoom_bubble_->ShowForReason(reason, /* allow_refocus_alert */ false);
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

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
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
    content::WebContents* web_contents,
    DisplayReason reason,
    ImmersiveModeController* immersive_mode_controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      auto_close_duration_(kBubbleCloseDelayDefault),
      auto_close_(reason == AUTOMATIC),
      immersive_mode_controller_(immersive_mode_controller),
      session_id_(chrome::FindBrowserWithTab(web_contents)->session_id()) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  SetNotifyEnterExitOnChild(true);
  if (immersive_mode_controller_)
    immersive_mode_controller_->AddObserver(this);
  UseCompactMargins();
}

ZoomBubbleView::~ZoomBubbleView() {
  if (immersive_mode_controller_)
    immersive_mode_controller_->RemoveObserver(this);
}

std::u16string ZoomBubbleView::GetAccessibleWindowTitle() const {
  Browser* browser = GetBrowser();
  if (!browser)
    return {};
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetPageActionIconView(PageActionIconType::kZoom)
      ->GetTextForTooltipAndAccessibleName();
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
      event->type() != ui::EventType::kGestureTap) {
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
  constexpr int kPercentLabelPadding = 64;
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  gfx::Insets inset_border_insets =
      provider->GetInsetsMetric(INSETS_TOAST) - margins();
  inset_border_insets.set_top_bottom(0, 0);
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, inset_border_insets, spacing);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));

  // Calculate child views margins in |this| client view.
  const int label_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL);
  const auto label_margin =
      gfx::Insets::TLBR(label_vertical_spacing - margins().top(), 0,
                        label_vertical_spacing - margins().bottom(),
                        kPercentLabelPadding - spacing);

  // Account for the apparent margins that vector buttons have around icons.
  const int control_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL);
  const auto control_vertical_margin =
      gfx::Insets::TLBR(control_vertical_spacing - margins().top(), 0,
                        control_vertical_spacing - margins().bottom(), 0);
  const gfx::Insets vector_button_margin(
      control_vertical_margin -
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON));

  const auto button_pressed_callback = [this](base::RepeatingClosure closure) {
    return base::BindRepeating(&ZoomBubbleView::ButtonPressed,
                               base::Unretained(this), std::move(closure));
  };

  // If this zoom change was initiated by an extension, that extension will be
  // attributed by showing its icon in the zoom bubble.
  if (extension_info_.icon_image) {
    auto image_button = std::make_unique<views::ImageButton>(
        button_pressed_callback(base::BindRepeating(
            &ZoomBubbleView::ImageButtonPressed, base::Unretained(this))));
    image_button->SetTooltipText(
        l10n_util::GetStringFUTF16(IDS_TOOLTIP_ZOOM_EXTENSION_ICON,
                                   base::UTF8ToUTF16(extension_info_.name)));
    image_button->SetImageModel(views::Button::STATE_NORMAL,
                                ui::ImageModel::FromImageSkia(
                                    extension_info_.icon_image->image_skia()));
    image_button_ = AddChildView(std::move(image_button));
  }

  // Add zoom label with the new zoom percent.
  auto label = std::make_unique<ZoomValue>(web_contents());
  label->SetProperty(views::kMarginsKey, gfx::Insets(label_margin));
  label->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
  label_ = label.get();
  AddChildView(std::move(label));

  const auto zoom_callback = [button_pressed_callback,
                              web_contents =
                                  web_contents()](content::PageZoom zoom) {
    return button_pressed_callback(base::BindRepeating(
        &zoom::PageZoom::Zoom, base::Unretained(web_contents), zoom));
  };

  // Add Zoom Out ("-") button.
  zoom_out_button_ =
      AddChildView(CreateZoomButton(zoom_callback(content::PAGE_ZOOM_OUT),
                                    kRemoveIcon, IDS_ACCNAME_ZOOM_MINUS2));
  zoom_out_button_->SetProperty(views::kMarginsKey,
                                gfx::Insets(vector_button_margin));

  // Add Zoom In ("+") button.
  zoom_in_button_ = AddChildView(CreateZoomButton(
      zoom_callback(content::PAGE_ZOOM_IN), kAddIcon, IDS_ACCNAME_ZOOM_PLUS2));
  zoom_in_button_->SetProperty(views::kMarginsKey,
                               gfx::Insets(vector_button_margin));

  // Add "Reset" button.
  auto reset_button = std::make_unique<views::MdTextButton>(
      zoom_callback(content::PAGE_ZOOM_RESET),
      l10n_util::GetStringUTF16(IDS_ZOOM_SET_DEFAULT));
  reset_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM_SET_DEFAULT));
  reset_button_ = AddChildView(std::move(reset_button));

  UpdateZoomPercent();
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
  Browser* browser = GetBrowser();
  if (ignore_close_bubble_ && browser &&
      GetAnchorViewForBrowser(browser) == GetAnchorView()) {
    return;
  }

  // Widget's Close() is async, but we don't want to use zoom_bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  zoom_bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

void ZoomBubbleView::OnImmersiveRevealStarted() {
  CloseBubble();
}

void ZoomBubbleView::OnImmersiveModeControllerDestroyed() {
  immersive_mode_controller_ = nullptr;
}

void ZoomBubbleView::OnExtensionIconImageChanged(
    extensions::IconImage* /* image */) {
  image_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(extension_info_.icon_image->image_skia()));
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
  // is an action icon (size-16) this is an acceptable alternative.
  const ExtensionIconSet* icons = &extensions::IconsInfo::GetIcons(extension);
  bool has_default_sized_icon =
      !icons->Get(gfx::kFaviconSize, ExtensionIconSet::Match::kExactly).empty();

  if (!has_default_sized_icon) {
    const extensions::ActionInfo* action =
        extensions::ActionInfo::GetExtensionActionInfo(extension);
    if (!action || action->default_icon.empty())
      return;  // Out of options.

    icons = &action->default_icon;
    icon_size = icons->map().begin()->first;
  }

  extension_info_.icon_image = std::make_unique<extensions::IconImage>(
      web_contents()->GetBrowserContext(), extension, *icons, icon_size,
      default_extension_icon_image, this);
}

void ZoomBubbleView::UpdateZoomPercent() {
  label_->SetText(base::FormatPercent(
      zoom::ZoomController::FromWebContents(web_contents())->GetZoomPercent()));
  label_->GetViewAccessibility().SetName(GetAccessibleWindowTitle());
  label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);

  // Disable buttons at min, max and default
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  double current_zoom_level = zoom_controller->GetZoomLevel();
  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  std::vector<double> zoom_levels =
      zoom::PageZoom::PresetZoomLevels(default_zoom_level);
  DCHECK(zoom_out_button_);
  zoom_out_button_->SetEnabled(
      !blink::ZoomValuesEqual(zoom_levels.front(), current_zoom_level));
  DCHECK(zoom_in_button_);
  zoom_in_button_->SetEnabled(
      !blink::ZoomValuesEqual(zoom_levels.back(), current_zoom_level));
}

void ZoomBubbleView::UpdateZoomIconVisibility() {
  // Note that we can't rely on web_contents() here, as it may have been
  // destroyed by the time we get this call. Also note parent_window() (if set)
  // may also be destroyed: the call to WindowClosing() may be triggered by
  // parent window destruction tearing down its child windows.
  Browser* browser = chrome::FindBrowserWithID(session_id_);
  if (browser && browser->window())
    browser->window()->UpdatePageActionIcon(PageActionIconType::kZoom);
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

void ZoomBubbleView::ButtonPressed(base::RepeatingClosure closure) {
  // No button presses in this dialog should cause the dialog to close,
  // including when the zoom level is set to 100% as a result of a button press.
  base::AutoReset<bool> auto_ignore_close_bubble(&ignore_close_bubble_, true);

  // Extend the timer to give a user more time after any button is pressed.
  auto_close_duration_ = kBubbleCloseDelayLong;
  StartTimerIfNecessary();

  closure.Run();
}

void ZoomBubbleView::ImageButtonPressed() {
  DCHECK(extension_info_.icon_image) << "Invalid button press.";
  Browser* browser = GetBrowser();
  if (browser) {
    chrome::AddSelectedTabWithURL(
        browser,
        GURL(base::StringPrintf("chrome://extensions?id=%s",
                                extension_info_.id.c_str())),
        ui::PAGE_TRANSITION_FROM_API);
  }
}

Browser* ZoomBubbleView::GetBrowser() const {
  return web_contents() ? chrome::FindBrowserWithTab(web_contents()) : nullptr;
}

ZoomBubbleView::ZoomBubbleExtensionInfo::ZoomBubbleExtensionInfo() {}

ZoomBubbleView::ZoomBubbleExtensionInfo::~ZoomBubbleExtensionInfo() {}

BEGIN_METADATA(ZoomBubbleView)
END_METADATA
