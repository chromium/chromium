// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
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
#include "base/strings/string_piece.h"
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

}  // namespace

// static
int PopupBaseView::GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteDeleteButton)
          ? views::Emphasis::kHigh
          : views::Emphasis::kMedium);
}

// static
int PopupBaseView::GetHorizontalMargin() {
  // The horizontal margin should match the offset of the bubble arrow (if
  // that arrow happens to be shown on the top).
  return views::BubbleBorder::kVisibleArrowBuffer;
}

// static
int PopupBaseView::GetHorizontalPadding() {
  // TODO(crbug.com/1411172): Combine with `GetHorizontalMargin`.
  return GetHorizontalMargin();
}

// The widget that the PopupBaseView will be attached to.
class PopupBaseView::Widget : public views::Widget {
 public:
  explicit Widget(PopupBaseView* autofill_popup_base_view) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = autofill_popup_base_view;
    params.parent = autofill_popup_base_view->GetParentNativeView();
    // Ensure the popup border is not painted on an opaque background.
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
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
    // All move events go into the parent, so that there is no covering at all
    // and mouse enter/exit events are detected and triggered properly.
    if (event->type() == ui::EventType::ET_MOUSE_MOVED && parent()) {
      parent()->SynthesizeMouseMoveEvent();
    }

    views::Widget::OnMouseEvent(event);
  }
};

PopupBaseView::PopupBaseView(
    base::WeakPtr<AutofillPopupViewDelegate> delegate,
    views::Widget* parent_widget,
    base::span<const views::BubbleArrowSide> preferred_popup_sides,
    bool show_arrow_pointer)
    : delegate_(delegate),
      parent_widget_(parent_widget),
      preferred_popup_sides_(
          {preferred_popup_sides.begin(), preferred_popup_sides.end()}),
      show_arrow_pointer_(show_arrow_pointer) {}

PopupBaseView::~PopupBaseView() {
  if (delegate_) {
    delegate_->ViewDestroyed();

    RemoveWidgetObservers();
  }

  CHECK(!IsInObserverList());
}

Browser* PopupBaseView::GetBrowser() {
  if (content::WebContents* web_contents = GetWebContents()) {
    return chrome::FindBrowserWithWebContents(web_contents);
  }
  return nullptr;
}

bool PopupBaseView::DoShow() {
  const bool initialize_widget = !GetWidget();
  if (initialize_widget) {
    // On Mac Cocoa browser, |parent_widget_| is null (the parent is not a
    // views::Widget).
    // TODO(crbug.com/826862): Remove |parent_widget_|.
    if (parent_widget_) {
      parent_widget_->AddObserver(this);
    }

    // The widget is destroyed by the corresponding NativeWidget, so we don't
    // have to worry about deletion.
    new PopupBaseView::Widget(this);
  }

  GetWidget()->GetRootView()->SetBorder(CreateBorder());
  bool enough_height = DoUpdateBoundsAndRedrawPopup();
  // If there is insufficient height, DoUpdateBoundsAndRedrawPopup() hides and
  // thus deletes |this|. Hence, there is nothing else to do.
  if (!enough_height) {
    return false;
  }

  if (content::WebContents* web_contents = GetWebContents()) {
    custom_cursor_blocker_ = web_contents->CreateDisallowCustomCursorScope(
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
  constexpr auto kDerivedClasses = base::MakeFixedFlatSet<base::StringPiece>(
      {"PopupSuggestionView", "PopupPasswordSuggestionView", "PopupFooterView",
       "PopupSeparatorView", "PopupWarningView", "PopupBaseView",
       "PasswordGenerationPopupViewViews::GeneratedPasswordBox",
       "PopupCellView"});
  DCHECK(kDerivedClasses.contains(selected_view.GetClassName()))
      << "If you add a new derived class from AutofillPopupRowView, add it "
         "here and to onSelection(evt) in "
         "chrome/browser/resources/chromeos/accessibility/chromevox/background/"
         "desktop_automation_handler.js to ensure that ChromeVox announces "
         "the item when selected. Missing class: "
      << selected_view.GetClassName();
#endif
  selected_view.NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void PopupBaseView::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  CHECK(widget == parent_widget_ || widget == GetWidget());
  if (widget != parent_widget_) {
    return;
  }

  HideController(PopupHidingReason::kWidgetChanged);
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

  HideController(PopupHidingReason::kWidgetChanged);
}

void PopupBaseView::RemoveWidgetObservers() {
  if (parent_widget_) {
    parent_widget_->RemoveObserver(this);
  }
  GetWidget()->RemoveObserver(this);
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

gfx::Rect PopupBaseView::GetOptionalPositionAndPlaceArrowOnPopup(
    const gfx::Rect& element_bounds,
    const gfx::Rect& max_bounds_for_popup,
    const gfx::Size& preferred_size) {
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
      /*popup_bounds=*/popup_bounds, preferred_popup_sides_);

  // Those values are not supported for adding an arrow.
  // Currently, they can not be returned by GetOptimalPopupPlacement().
  DCHECK(arrow != views::BubbleBorder::Arrow::NONE);
  DCHECK(arrow != views::BubbleBorder::Arrow::FLOAT);

  if (show_arrow_pointer_) {
    // Set the arrow position to the border.
    border->set_arrow(arrow);
    border->AddArrowToBubbleCornerAndPointTowardsAnchor(element_bounds,
                                                        popup_bounds);
  }

  return popup_bounds;
}

bool PopupBaseView::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = GetPreferredSize();
  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  // TODO(crbug.com/1262371) Once popups can render outside the main window on
  // Linux, use the screen bounds.
  const gfx::Rect top_window_bounds = GetTopWindowBounds();
  const gfx::Rect& max_bounds_for_popup =
      PopupMayExceedContentAreaBounds(GetWebContents()) ? top_window_bounds
                                                        : content_area_bounds;

  gfx::Rect element_bounds = gfx::ToEnclosingRect(delegate_->element_bounds());

  // If the element exceeds the content area, ensure that the popup is still
  // visually attached to the input element.
  element_bounds.Intersect(content_area_bounds);
  if (element_bounds.IsEmpty()) {
    HideController(PopupHidingReason::kElementOutsideOfContentArea);
    return false;
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
    HideController(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  gfx::Rect popup_bounds = GetOptionalPositionAndPlaceArrowOnPopup(
      element_bounds, max_bounds_for_popup, preferred_size);

  if (BoundsOverlapWithPictureInPictureWindow(popup_bounds)) {
    HideController(PopupHidingReason::kOverlappingWithPictureInPictureWindow);
    return false;
  }

  // Account for the scroll view's border so that the content has enough space.
  popup_bounds.Inset(-GetWidget()->GetRootView()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);

  Layout();
  UpdateClipPath();
  SchedulePaint();
  return true;
}

std::unique_ptr<views::Border> PopupBaseView::CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      ui::kColorDropdownBackground);
  border->SetCornerRadius(GetCornerRadius());
  views::Emphasis emphasis =
      base::FeatureList::IsEnabled(features::kAutofillMoreProminentPopup)
          ? views::Emphasis::kMaximum
          : views::Emphasis::kMedium;
  border->set_md_shadow_elevation(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(emphasis));
  return border;
}

void PopupBaseView::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (GetWidget() && GetWidget()->GetNativeView() != focused_now) {
    HideController(PopupHidingReason::kFocusChanged);
  }
}

void PopupBaseView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(aleventhal) The correct role spec-wise to use here is kMenu, however
  // as of NVDA 2018.2.1, firing a menu event with kMenu breaks left/right
  // arrow editing feedback in text field. If NVDA addresses this we should
  // consider returning to using kMenu, so that users are notified that a
  // menu popup has been shown.
  node_data->role = ax::mojom::Role::kPane;
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void PopupBaseView::HideController(PopupHidingReason reason) {
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

gfx::NativeView PopupBaseView::GetParentNativeView() const {
  return parent_widget_ ? parent_widget_->GetNativeView()
                        : delegate_->container_view();
}

gfx::NativeView PopupBaseView::container_view() {
  return delegate_->container_view();
}

BEGIN_METADATA(PopupBaseView, views::WidgetDelegateView)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ContentAreaBounds)
END_METADATA

}  // namespace autofill
