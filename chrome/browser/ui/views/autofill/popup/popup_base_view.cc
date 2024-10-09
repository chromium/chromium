// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

#if DCHECK_IS_ON()
#include "base/containers/fixed_flat_set.h"
#endif

namespace autofill {

namespace {

// The maximum size (in DIPs) of custom cursors that are permitted while the
// popup is shown. The size is limited to avoid custom cursors that cover most
// of the popup.
constexpr int kMaximumAllowedCustomCursorDimension = 24;

// The maximum number of pixels the suggestions dialog is shifted towards the
// center the focused field.
constexpr int kMaximumPixelsToMoveSuggestionToCenter = 120;

// The maximum width percentage the suggestion dialog is shifted towards the
// center of the focused field.
constexpr int kMaximumWidthPercentageToMoveTheSuggestionToCenter = 50;

// The max number of pixels the popup is allowed to be rendered above the top
// of the `WebContents`. Limiting overflow prevents the popup content from
// covering important browser elements (e.g., the address bar).
constexpr int kMaxPopupWebContentsTopYOverflow = 8;

// Creates a border for a popup.
std::unique_ptr<views::Border> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      ui::kColorDropdownBackground);
  border->SetCornerRadius(PopupBaseView::GetCornerRadius());
  border->set_md_shadow_elevation(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(
          base::FeatureList::IsEnabled(features::kAutofillMoreProminentPopup)
              ? views::Emphasis::kMaximum
              : views::Emphasis::kMedium));
  return border;
}

}  // namespace

// static
int PopupBaseView::GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

// static
int PopupBaseView::ArrowHorizontalMargin() {
  // The horizontal margin should match the offset of the bubble arrow (if
  // that arrow happens to be shown on the top).
  return views::BubbleBorder::kVisibleArrowBuffer;
}

// The widget that the PopupBaseView will be attached to.
class PopupBaseView::Widget : public views::Widget {
 public:
  // Takes ownership of `autofill_popup_base_view` and uses it as the delegate
  // of a new Widget. `parent_native_view` is the intended parent view of the
  // new Widget.
  explicit Widget(PopupBaseView* autofill_popup_base_view,
                  gfx::NativeView parent_native_view,
                  views::Widget::InitParams::Activatable activatable) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.delegate = autofill_popup_base_view;
    params.parent = parent_native_view;
    // Ensure the popup border is not painted on an opaque background.
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    params.activatable = activatable;

    // `kSecuritySurface` makes the popup to display on top of all other windows
    // (including system ones, but the support among different OS, versions and
    // setups is not consistent). This is not required for regular autofill
    // popup use, but it makes certain attacks (those based on the popup being
    // obscured) less practical.
    if (base::FeatureList::IsEnabled(
            features::kAutofillPopupZOrderSecuritySurface)) {
      params.z_order = ui::ZOrderLevel::kSecuritySurface;
    }

    Init(std::move(params));
    AddObserver(popup_base_view());

    // No animation for popup appearance (too distracting).
    SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);
  }

  PopupBaseView* popup_base_view() const {
    // This cast is always safe since we pass the base view as a delegate.
    return static_cast<PopupBaseView*>(widget_delegate());
  }

  // views::Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    if (!popup_base_view() || popup_base_view()->GetBrowser()) {
      return nullptr;
    }

    return &ThemeService::GetThemeProviderForProfile(
        popup_base_view()->GetBrowser()->profile());
  }

  views::Widget* GetPrimaryWindowWidget() override {
    if (!popup_base_view() || !popup_base_view()->GetBrowser()) {
      return nullptr;
    }

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(popup_base_view()->GetBrowser());
    if (!browser_view) {
      return nullptr;
    }

    return browser_view->GetWidget()->GetPrimaryWindowWidget();
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    views::View* parent_content_view =
        parent() ? parent()->GetContentsView() : nullptr;

    if (!parent_content_view) {
      views::Widget::OnMouseEvent(event);
      return;
    }

    // Suppress the exit event on MacOS and Windows generated when the sub-popup
    // initially opens. We assume that it is the sub-popup that hovers
    // the parent by its semi-transparent shadow part. But in theory it could be
    // another window, which is not a problem because the popup closes on focus
    // loss anyway. The exit event will be synthesized by the sub-popup later
    // (find the trick that does this below).
    if (event->type() == ui::EventType::kMouseExited &&
        GetContentsView()->IsMouseHovered()) {
      return;
    }

    // Retrigger mouse moves on the parent to make selection/highlighting work
    // properly and thus provide more intuitive UX when the child's transparent
    // parts (e.g. shadow) overlap the parent (assuming that the child contents
    // view is not overlapped).
    if (event->type() == ui::EventType::kMouseMoved &&
        !GetContentsView()->IsMouseHovered() &&
        parent_content_view->IsMouseHovered()) {
      parent()->SynthesizeMouseMoveEvent();
      // Save the synthesized event position to use it for the exit event
      // later.
      last_synthesized_parent_mouse_move_position_ =
          display::Screen::GetScreen()->GetCursorScreenPoint();
    } else if (!parent_content_view->IsMouseHovered() &&
               last_synthesized_parent_mouse_move_position_.has_value()) {
      // Generate the exit event after a set of move events as there is no one
      // handling this case (when the mouse gets outside of the parent
      // widget), which is important for the selection/highlighting state
      // consistency.
      const gfx::Point location = View::ConvertPointFromScreen(
          parent()->GetRootView(),
          last_synthesized_parent_mouse_move_position_.value());
      ui::MouseEvent mouse_event(ui::EventType::kMouseExited, location,
                                 location, ui::EventTimeForNow(),
                                 ui::EF_IS_SYNTHESIZED,
                                 /*changed_button_flags=*/0);
      parent()->OnMouseEvent(&mouse_event);
      last_synthesized_parent_mouse_move_position_.reset();
    }

    views::Widget::OnMouseEvent(event);
  }

 private:
  std::optional<gfx::Point> last_synthesized_parent_mouse_move_position_;
};

PopupBaseView::PopupBaseView(
    base::WeakPtr<AutofillPopupViewDelegate> delegate,
    views::Widget* parent_widget,
    views::Widget::InitParams::Activatable new_widget_activatable,
    bool show_arrow_pointer)
    : delegate_(delegate),
      parent_widget_(parent_widget),
      new_widget_activatable_(new_widget_activatable),
      show_arrow_pointer_(show_arrow_pointer) {
  // TODO(aleventhal) The correct role spec-wise to use here is kMenu, however
  // as of NVDA 2018.2.1, firing a menu event with kMenu breaks left/right
  // arrow editing feedback in text field. If NVDA addresses this we should
  // consider returning to using kMenu, so that users are notified that a
  // menu popup has been shown.
  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

PopupBaseView::~PopupBaseView() {
  if (delegate_) {
    delegate_->ViewDestroyed();
  }
  RemoveWidgetObservers();

  CHECK(!IsInObserverList());
}

Browser* PopupBaseView::GetBrowser() {
  if (content::WebContents* web_contents = GetWebContents()) {
    return chrome::FindBrowserWithTab(web_contents);
  }
  return nullptr;
}

bool PopupBaseView::DoShow() {
  const bool initialize_widget = !GetWidget();
  if (initialize_widget) {
    // On Mac Cocoa browser, |parent_widget_| is null (the parent is not a
    // views::Widget).
    // TODO(crbug.com/41379554): Remove |parent_widget_|.
    if (parent_widget_) {
      parent_widget_->AddObserver(this);
    }

    // The widget is destroyed by the corresponding NativeWidget, so we don't
    // have to worry about deletion.
    new PopupBaseView::Widget(this, /*parent_native_view=*/
                              parent_widget_ ? parent_widget_->GetNativeView()
                                             : delegate_->container_view(),
                              new_widget_activatable_);
  }

  GetWidget()->GetRootView()->SetBorder(CreateBorder());
  bool enough_height = DoUpdateBoundsAndRedrawPopup();
  // If there is insufficient height, DoUpdateBoundsAndRedrawPopup() hides and
  // thus deletes |this|. Hence, there is nothing else to do.
  if (!enough_height) {
    return false;
  }

  if (GetWebContents()) {
    custom_cursor_suppressor_.Start(
        /*max_dimension_dips=*/kMaximumAllowedCustomCursorDimension + 1);
  } else {
    // `delegate_` is already gone and `WebContents` is destroying itself.
    return false;
  }

  GetWidget()->Show();

  // Showing the widget can change native focus (which would result in an
  // immediate hiding of the popup). Only start observing after shown.
  if (initialize_widget) {
    CHECK(!focus_observation_.IsObserving());
    focus_observation_.Observe(views::WidgetFocusManager::GetInstance());
  }

  return true;
}

void PopupBaseView::DoHide() {
  if (is_ax_menu_start_event_fired_) {
    // Fire menu end event.
    // The menu start event is delayed until the user
    // navigates into the menu, otherwise some screen readers will ignore
    // any focus events outside of the menu, including a focus event on
    // the form control itself.
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupEnd, true);
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
    GetViewAccessibility().EndPopupFocusOverride();

    // Also fire an accessible focus event on what currently has focus,
    // typically the widget associated with this popup.
    if (parent_widget_) {
      if (views::FocusManager* focus_manager =
              parent_widget_->GetFocusManager()) {
        if (View* focused_view = focus_manager->GetFocusedView()) {
          focused_view->GetViewAccessibility().FireFocusAfterMenuClose();
        }
      }
    }
  }

  // The controller is no longer valid after it hides us.
  delegate_ = nullptr;

  RemoveWidgetObservers();

  if (GetWidget()) {
    // Don't call CloseNow() because some of the functions higher up the stack
    // assume the the widget is still valid after this point.
    // http://crbug.com/229224
    // NOTE: This deletes |this|.
    GetWidget()->Close();
  } else {
    delete this;
  }
}

void PopupBaseView::NotifyAXSelection(views::View& selected_view) {
  if (!is_ax_menu_start_event_fired_) {
    // Fire the menu start event once, right before the first item is selected.
    // By firing these and the matching kMenuEnd events, we are telling screen
    // readers that the focus is only changing temporarily, and the screen
    // reader will restore the focus back to the appropriate textfield when the
    // menu closes.
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupStart, true);

    is_ax_menu_start_event_fired_ = true;
  }
  selected_view.GetViewAccessibility().SetPopupFocusOverride();
#if DCHECK_IS_ON()
  // TODO(crbug.com/362445293): Update the automation handler once the
  // Typescript migration is complete.
  constexpr auto kDerivedClasses = base::MakeFixedFlatSet<std::string_view>(
      {"PopupSuggestionView", "PopupPasswordSuggestionView", "PopupFooterView",
       "PopupSeparatorView", "PopupWarningView", "PopupBaseView",
       "PasswordGenerationPopupViewViews::GeneratedPasswordBox", "PopupRowView",
       "PopupRowWithButtonView", "PopupRowContentView", "MdTextButton",
       "PopupRowPredictionImprovementsFeedbackView"});
  DCHECK(kDerivedClasses.contains(selected_view.GetClassName()))
      << "If you add a new derived class from AutofillPopupRowView, add it "
         "here and to onSelection(evt) in "
         "chrome/browser/resources/chromeos/accessibility/chromevox/background/"
         "event/desktop_automation_handler.js to ensure that ChromeVox "
         "announces the item when selected. Missing class: "
      << selected_view.GetClassName();
#endif
}

void PopupBaseView::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  CHECK(widget == parent_widget_ || widget == GetWidget());
  if (widget != parent_widget_) {
    return;
  }

  HideController(SuggestionHidingReason::kWidgetChanged);
}

void PopupBaseView::OnWidgetDestroying(views::Widget* widget) {
  // On Windows, widgets can be destroyed in any order. Regardless of which
  // widget is destroyed first, remove all observers and hide the popup.
  CHECK(widget == parent_widget_ || widget == GetWidget());

  // Normally this happens at destruct-time or hide-time, but because it depends
  // on |parent_widget_| (which is about to go away), it needs to happen sooner
  // in this case.
  RemoveWidgetObservers();

  // Because the parent widget is about to be destroyed, we null out the weak
  // reference to it and protect against possibly accessing it during
  // destruction (e.g., by attempting to remove observers).
  parent_widget_ = nullptr;

  HideController(SuggestionHidingReason::kWidgetChanged);
}

void PopupBaseView::RemoveWidgetObservers() {
  if (parent_widget_) {
    parent_widget_->RemoveObserver(this);
  }
  if (views::Widget* widget = GetWidget()) {
    widget->RemoveObserver(this);
  }
  focus_observation_.Reset();
}

void PopupBaseView::UpdateClipPath() {
  SkRect local_bounds = gfx::RectToSkRect(GetLocalBounds());
  SkScalar radius = SkIntToScalar(GetCornerRadius());
  SkPath clip_path;
  clip_path.addRoundRect(local_bounds, radius, radius);
  SetClipPath(clip_path);
}

gfx::Rect PopupBaseView::GetContentAreaBounds() const {
  content::WebContents* web_contents = GetWebContents();
  if (web_contents) {
    return web_contents->GetContainerBounds();
  }

  // If the |web_contents| is null, simply return an empty rect. The most common
  // reason to end up here is that the |web_contents| has been destroyed
  // externally, which can happen at any time. This happens fairly commonly on
  // Windows (e.g., at shutdown) in particular.
  return gfx::Rect();
}

gfx::Rect PopupBaseView::GetTopWindowBounds() const {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      delegate_->container_view());
  // Find root in window tree.
  while (widget && widget->parent()) {
    widget = widget->parent();
  }
  if (widget) {
    return widget->GetWindowBoundsInScreen();
  }

  // If the widget is null, simply return an empty rect. The most common reason
  // to end up here is that the NativeView has been destroyed externally, which
  // can happen at any time. This happens fairly commonly on Windows (e.g., at
  // shutdown) in particular.
  return gfx::Rect();
}

gfx::Rect PopupBaseView::GetOptimalPositionAndPlaceArrowOnPopup(
    const gfx::Rect& element_bounds,
    const gfx::Rect& max_bounds_for_popup,
    const gfx::Size& preferred_size,
    base::span<const views::BubbleArrowSide> preferred_popup_sides) {
  views::BubbleBorder* border = static_cast<views::BubbleBorder*>(
      GetWidget()->GetRootView()->GetBorder());
  DCHECK(border);

  gfx::Rect popup_bounds;

  int maximum_pixel_offset_to_center =
      base::FeatureList::IsEnabled(features::kAutofillMoreProminentPopup)
          ? features::kAutofillMoreProminentPopupMaxOffsetToCenterParam.Get()
          : kMaximumPixelsToMoveSuggestionToCenter;

  // Deduce the arrow and the position.
  views::BubbleBorder::Arrow arrow = GetOptimalPopupPlacement(
      /*content_area_bounds=*/max_bounds_for_popup,
      /*element_bounds=*/element_bounds,
      /*popup_preferred_size=*/preferred_size,
      /*right_to_left=*/delegate_->GetElementTextDirection() ==
          base::i18n::TextDirection::RIGHT_TO_LEFT,
      /*scrollbar_width=*/gfx::scrollbar_size(),
      /*maximum_pixel_offset_to_center=*/
      maximum_pixel_offset_to_center,
      /*maximum_width_percentage_to_center=*/
      kMaximumWidthPercentageToMoveTheSuggestionToCenter,
      /*popup_bounds=*/popup_bounds, preferred_popup_sides,
      /*anchor_type=*/delegate_->anchor_type());

  // Those values are not supported for adding an arrow.
  // Currently, they can not be returned by GetOptimalPopupPlacement().
  DCHECK(arrow != views::BubbleBorder::Arrow::NONE);
  DCHECK(arrow != views::BubbleBorder::Arrow::FLOAT);

  if (show_arrow_pointer_) {
    // Set the arrow position to the border.
    border->set_arrow(arrow);
    border->AddArrowToBubbleCornerAndPointTowardsAnchor(
        element_bounds, popup_bounds,
        max_bounds_for_popup.y() - kMaxPopupWebContentsTopYOverflow);
  }

  return popup_bounds;
}

bool PopupBaseView::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = GetPreferredSize();
  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  // TODO(crbug.com/40799454) Once popups can render outside the main window on
  // Linux, use the screen bounds.
  const gfx::Rect top_window_bounds = GetTopWindowBounds();
  const gfx::Rect& max_bounds_for_popup =
      PopupMayExceedContentAreaBounds(GetWebContents()) ? top_window_bounds
                                                        : content_area_bounds;

  gfx::Rect element_bounds = gfx::ToEnclosingRect(delegate_->element_bounds());

  // An element that is contained by the `content_area_bounds` (even if empty,
  // which means either the height or the width is 0) is never outside the
  // content area. An empty element case can happen with caret bounds, which
  // sometimes has 0 width.
  if (!content_area_bounds.Contains(element_bounds)) {
    // If the element exceeds the content area, ensure that the popup is still
    // visually attached to the input element.
    element_bounds.Intersect(content_area_bounds);
    if (element_bounds.IsEmpty()) {
      HideController(SuggestionHidingReason::kElementOutsideOfContentArea);
      return false;
    }
  }
  // Consider the element is |kElementBorderPadding| pixels larger at the top
  // and at the bottom in order to reposition the dropdown, so that it doesn't
  // look too close to the element.
  element_bounds.Inset(
      gfx::Insets::VH(/*vertical=*/-kElementBorderPadding, /*horizontal=*/0));

  // At least one row of the popup should be shown in the bounds of the content
  // area so that the user notices the presence of the popup.
  int item_height =
      children().size() > 0 ? children()[0]->GetPreferredSize().height() : 0;
  if (!CanShowDropdownHere(item_height, max_bounds_for_popup, element_bounds)) {
    HideController(SuggestionHidingReason::kInsufficientSpace);
    return false;
  }

  gfx::Rect popup_bounds = GetOptimalPositionAndPlaceArrowOnPopup(
      element_bounds, max_bounds_for_popup, preferred_size,
      kDefaultPreferredPopupSides);

  if (BoundsOverlapWithPictureInPictureWindow(popup_bounds)) {
    HideController(
        SuggestionHidingReason::kOverlappingWithPictureInPictureWindow);
    return false;
  }

  // Account for the scroll view's border so that the content has enough space.
  popup_bounds.Inset(-GetWidget()->GetRootView()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);

  DeprecatedLayoutImmediately();
  UpdateClipPath();
  SchedulePaint();
  return true;
}

void PopupBaseView::OnNativeFocusChanged(gfx::NativeView focused_now) {
  // TODO(crbug.com/330303918): The focus change is triggered sometimes
  // (reproduced on a Linux release build, on a debug one - no) with
  // `focused_now` == `nullptr` during activatable popup opening, no other
  // widget gets focus then and this widget remains active.
  // The `!GetWidget()->IsActive()` piece handles this case and prevents
  // immediate popup closing.
  // Investigate the reason and either fix it on the appropriate side or make
  // this TODO a regular comment if it works as intended.
  if (GetWidget() && GetWidget()->GetNativeView() != focused_now &&
      !GetWidget()->IsActive()) {
    HideController(SuggestionHidingReason::kFocusChanged);
  }
}

void PopupBaseView::HideController(SuggestionHidingReason reason) {
  if (delegate_) {
    delegate_->Hide(reason);
  }
  // This will eventually result in the deletion of |this|, as the delegate
  // will hide |this|. See |DoHide| above for an explanation on why the precise
  // timing of that deletion is tricky.
}

content::WebContents* PopupBaseView::GetWebContents() const {
  if (!delegate_) {
    return nullptr;
  }

  return delegate_->GetWebContents();
}

BEGIN_METADATA(PopupBaseView)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ContentAreaBounds)
END_METADATA

}  // namespace autofill
