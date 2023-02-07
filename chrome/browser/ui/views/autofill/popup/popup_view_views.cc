// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using autofill::PopupItemId;
using views::BubbleBorder;

namespace autofill {

namespace {

// By spec, dropdowns should always have a width which is a multiple of 12.
// TODO(crbug.com/1411172): Deduplicate this between here and `PopupRowView`.
constexpr int kAutofillPopupWidthMultiple = 12;
constexpr int kAutofillPopupMinWidth = 0;
// TODO(crbug.com/831603): move handling the max width to the base class.
constexpr int kAutofillPopupMaxWidth = kAutofillPopupWidthMultiple * 38;

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
}

// Returns true if the item at `line_number` is a footer item.
bool IsFooterItem(const std::vector<Suggestion>& suggestions,
                  size_t line_number) {
  if (line_number >= suggestions.size()) {
    return false;
  }

  switch (suggestions[line_number].frontend_id) {
    case PopupItemId::POPUP_ITEM_ID_SCAN_CREDIT_CARD:
    case PopupItemId::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
    case PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY:
    case PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN:
    case PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN:
    case PopupItemId::
        POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE:
    case PopupItemId::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS:
    case PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD:
    case PopupItemId::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
    case PopupItemId::POPUP_ITEM_ID_CLEAR_FORM:
    case PopupItemId::POPUP_ITEM_ID_AUTOFILL_OPTIONS:
    case PopupItemId::POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS:
      return true;
    // If the next item is a footer item, the separator also belongs to the
    // footer.
    case PopupItemId::POPUP_ITEM_ID_SEPARATOR:
      return IsFooterItem(suggestions, line_number + 1);
    default:
      return false;
  }
}

}  // namespace

PopupViewViews::PopupViewViews(
    base::WeakPtr<AutofillPopupController> controller,
    views::Widget* parent_widget)
    : PopupBaseView(controller, parent_widget), controller_(controller) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

  CreateChildViews();
}

PopupViewViews::~PopupViewViews() = default;

void PopupViewViews::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBox;
  // If controller_ is valid, then the view is expanded.
  if (controller_) {
    node_data->AddState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
  }
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void PopupViewViews::Show() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  DoShow();
}

void PopupViewViews::Hide() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  DoHide();
}

void PopupViewViews::OnSelectedRowChanged(
    absl::optional<int> previous_row_selection,
    absl::optional<int> current_row_selection) {
  if (previous_row_selection) {
    GetPopupRowViewAt(*previous_row_selection).SetSelected(false);
  }

  if (current_row_selection) {
    PopupRowView& current_row = GetPopupRowViewAt(*current_row_selection);
    current_row.SetSelected(true);
    current_row.ScrollViewToVisible();
  }

  NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
}

void PopupViewViews::OnSuggestionsChanged() {
  CreateChildViews();
  DoUpdateBoundsAndRedrawPopup();
}

absl::optional<int32_t> PopupViewViews::GetAxUniqueId() {
  return absl::optional<int32_t>(
      PopupBaseView::GetViewAccessibility().GetUniqueId());
}

void PopupViewViews::AxAnnounce(const std::u16string& text) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  browser_view->GetViewAccessibility().AnnounceText(text);
}

void PopupViewViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  if (visible) {
    for (RowPointer& row_view : rows_) {
      if (PopupRowView** row_view_pointer =
              absl::get_if<PopupRowView*>(&row_view)) {
        (*row_view_pointer)->MaybeShowIphPromo();
      }
    }
  }
}

void PopupViewViews::CreateChildViews() {
  // Null all pointers prior to deleting the children views to avoid temporarily
  // dangling pointers that might be picked up by dangle detection builds.
  scroll_view_ = nullptr;
  body_container_ = nullptr;
  rows_.clear();
  RemoveAllChildViews();

  const std::vector<Suggestion> kSuggestions = controller_->GetSuggestions();

  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  // `content_view` wraps the full content of the popup and provides vertical
  // padding. This is similar to `padding_wrapper` used in the scroll area, but
  // it allows to add a padding below the footer.
  raw_ptr<views::BoxLayoutView> content_view = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(
              gfx::Insets::VH(GetContentsVerticalPadding(), 0))
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .Build());

  rows_.reserve(kSuggestions.size());
  size_t current_line_number = 0u;
  // Add the body rows, if there are any.
  if (!kSuggestions.empty() && !IsFooterItem(kSuggestions, 0u)) {
    // Create a container to wrap the "regular" (non-footer) rows.
    std::unique_ptr<views::BoxLayoutView> body_container =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
            .Build();

    for (; current_line_number < kSuggestions.size() &&
           !IsFooterItem(kSuggestions, current_line_number);
         ++current_line_number) {
      int frontend_id = kSuggestions[current_line_number].frontend_id;
      switch (frontend_id) {
        case PopupItemId::POPUP_ITEM_ID_SEPARATOR:
          rows_.push_back(body_container->AddChildView(
              std::make_unique<PopupSeparatorView>()));
          break;

        case PopupItemId::POPUP_ITEM_ID_MIXED_FORM_MESSAGE:
        case PopupItemId::
            POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
          rows_.push_back(
              body_container->AddChildView(std::make_unique<PopupWarningView>(
                  kSuggestions[current_line_number])));
          break;

        case PopupItemId::POPUP_ITEM_ID_USERNAME_ENTRY:
        case PopupItemId::POPUP_ITEM_ID_PASSWORD_ENTRY:
        case PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY:
        case PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY:
          rows_.push_back(
              body_container->AddChildView(PopupPasswordSuggestionView::Create(
                  *this, current_line_number, frontend_id)));
          break;

        // The default section contains most of the suggestions including
        // addresses and credit cards.
        default:
          rows_.push_back(
              body_container->AddChildView(PopupSuggestionView::Create(
                  *this, current_line_number, frontend_id,
                  controller_->GetPopupType())));
      }
    }

    std::unique_ptr<views::ScrollView> scroll_view =
        views::Builder<views::ScrollView>()
            .SetBackgroundThemeColorId(ui::kColorDropdownBackground)
            .SetHorizontalScrollBarMode(
                views::ScrollView::ScrollBarMode::kDisabled)
            .SetDrawOverflowIndicator(false)
            .ClipHeightTo(0, body_container->GetPreferredSize().height())
            .Build();
    body_container_ = scroll_view->SetContents(std::move(body_container));
    scroll_view_ = content_view->AddChildView(std::move(scroll_view));
    content_view->SetFlexForView(scroll_view_.get(), 1);
  }

  if (current_line_number >= kSuggestions.size()) {
    return;
  }

  // Footer items need to be in their own container because they should not be
  // affected by scrolling behavior (they are "sticky" at the bottom) and
  // because they have a special background color
  std::unique_ptr<views::BoxLayoutView> footer_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .SetBackground(views::CreateThemedSolidBackground(
              ui::kColorBubbleFooterBackground))
          .Build();

  for (; current_line_number < kSuggestions.size(); ++current_line_number) {
    DCHECK(IsFooterItem(kSuggestions, current_line_number));
    // The footer can contain either footer views or separator lines.
    if (kSuggestions[current_line_number].frontend_id ==
        POPUP_ITEM_ID_SEPARATOR) {
      rows_.push_back(footer_container->AddChildView(
          std::make_unique<PopupSeparatorView>()));
    } else {
      rows_.push_back(footer_container->AddChildView(PopupFooterView::Create(
          *this, current_line_number,
          kSuggestions[current_line_number].frontend_id)));
    }
  }

  content_view->SetFlexForView(
      content_view->AddChildView(std::move(footer_container)), 0);
}

int PopupViewViews::AdjustWidth(int width) const {
  if (width >= kAutofillPopupMaxWidth) {
    return kAutofillPopupMaxWidth;
  }

  if (width <= kAutofillPopupMinWidth) {
    return kAutofillPopupMinWidth;
  }

  // The popup size is being determined by the contents, rather than the min/max
  // or the element bounds. Round up to a multiple of
  // |kAutofillPopupWidthMultiple|.
  if (width % kAutofillPopupWidthMultiple) {
    width +=
        (kAutofillPopupWidthMultiple - (width % kAutofillPopupWidthMultiple));
  }

  return width;
}

bool PopupViewViews::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = CalculatePreferredSize();
  gfx::Rect popup_bounds;

  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  // TODO(crbug.com/1262371) Once popups can render outside the main window on
  // Linux, use the screen bounds.
  const gfx::Rect top_window_bounds = GetTopWindowBounds();
  const gfx::Rect& max_bounds_for_popup =
      PopupMayExceedContentAreaBounds(controller_->GetWebContents())
          ? top_window_bounds
          : content_area_bounds;

  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());

  // If the element exceeds the content area, ensure that the popup is still
  // visually attached to the input element.
  element_bounds.Intersect(content_area_bounds);
  if (element_bounds.IsEmpty()) {
    controller_->Hide(PopupHidingReason::kElementOutsideOfContentArea);
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
      body_container_ && body_container_->children().size() > 0
          ? body_container_->children()[0]->GetPreferredSize().height()
          : 0;

  if (!CanShowDropdownHere(item_height, max_bounds_for_popup, element_bounds)) {
    controller_->Hide(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  CalculatePopupYAndHeight(preferred_size.height(), max_bounds_for_popup,
                           element_bounds, &popup_bounds);

  // Adjust the width to compensate for a scroll bar, if necessary, and for
  // other rules.
  int scroll_width = 0;
  if (scroll_view_ && preferred_size.height() > popup_bounds.height()) {
    preferred_size.set_height(popup_bounds.height());

    // Because the preferred size is greater than the bounds available, the
    // contents will have to scroll. The scroll bar will steal width from the
    // content and smoosh everything together. Instead, add to the width to
    // compensate.
    scroll_width = scroll_view_->GetScrollBarLayoutWidth();
  }
  preferred_size.set_width(AdjustWidth(preferred_size.width() + scroll_width));

  popup_bounds = GetOptionalPositionAndPlaceArrowOnPopup(
      element_bounds, content_area_bounds, preferred_size);

  if (BoundsOverlapWithAnyOpenPrompt(popup_bounds,
                                     controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }
  // On Windows, due to platform-specific implementation details, the previous
  // check isn't reliable, and fails to detect open prompts. Since the most
  // critical bubble is the permission bubble, we check for that specifically.
  if (BoundsOverlapWithOpenPermissionsPrompt(popup_bounds,
                                             controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }

  // The pip surface is given the most preference while rendering. So, the
  // autofill popup should not be shown when the picture in picture window
  // hides the autofill form behind it.
  // For more details on how this can happen, see crbug.com/1358647.
  if (BoundsOverlapWithPictureInPictureWindow(popup_bounds)) {
    controller_->Hide(
        PopupHidingReason::kOverlappingWithPictureInPictureWindow);
    return false;
  }

  SetSize(preferred_size);

  popup_bounds.Inset(-GetWidget()->GetRootView()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);
  UpdateClipPath();

  SchedulePaint();
  return true;
}

BEGIN_METADATA(PopupViewViews, PopupBaseView)
END_METADATA

// static
AutofillPopupView* AutofillPopupView::Create(
    base::WeakPtr<AutofillPopupController> controller) {
#if BUILDFLAG(IS_MAC)
  // It's possible for the container_view to not be in a window. In that case,
  // cancel the popup since we can't fully set it up.
  if (!platform_util::GetTopLevel(controller->container_view())) {
    return nullptr;
  }
#endif

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

#if !BUILDFLAG(IS_MAC)
  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up. On Mac Cocoa browser, |observing_widget| is null
  // because the parent is not a views::Widget.
  if (!observing_widget) {
    return nullptr;
  }
#endif

  return new PopupViewViews(controller, observing_widget);
}

}  // namespace autofill
