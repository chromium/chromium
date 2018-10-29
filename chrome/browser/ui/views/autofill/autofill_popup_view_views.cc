// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_views.h"

#include "base/feature_list.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// The minimum vertical space between the bottom of the autofill popup and the
// bottom of the Chrome frame.
// TODO(crbug.com/739978): Investigate if we should compute this distance
// programmatically. 10dp may not be enough for windows with thick borders.
const int kPopupBottomMargin = 10;

// The thickness of the border for the autofill popup in dp.
const int kPopupBorderThicknessDp = 1;

// Child view only for triggering accessibility events. Rendering is handled
// by |AutofillPopupViewViews|.
class AutofillPopupChildView : public views::View {
 public:
  explicit AutofillPopupChildView(const Suggestion& suggestion,
                                  int32_t set_size,
                                  int32_t pos_in_set)
      : suggestion_(suggestion),
        is_selected_(false),
        set_size_(set_size),
        pos_in_set_(pos_in_set) {
    SetFocusBehavior(suggestion.frontend_id == POPUP_ITEM_ID_SEPARATOR
                         ? FocusBehavior::NEVER
                         : FocusBehavior::ALWAYS);
  }

  void OnSelected() { is_selected_ = true; }

  void OnUnselected() { is_selected_ = false; }

 private:
  ~AutofillPopupChildView() override {}

  // views::Views implementation
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(suggestion_.value);

    bool is_separator = suggestion_.frontend_id == POPUP_ITEM_ID_SEPARATOR;
    if (is_separator) {
      // Separators are not selectable.
      node_data->role = ax::mojom::Role::kSplitter;
    } else {
      // Options are selectable.
      node_data->role = ax::mojom::Role::kMenuItem;
      node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                  is_selected_);
    }

    node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size_);
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set_);
  }

  const Suggestion& suggestion_;

  bool is_selected_;

  // Total number of suggestions.
  const int32_t set_size_;

  // Position of suggestion in list (1-based index).
  const int32_t pos_in_set_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupChildView);
};

}  // namespace

AutofillPopupViewViews::AutofillPopupViewViews(
    AutofillPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  CreateChildViews();
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

AutofillPopupViewViews::~AutofillPopupViewViews() {}

void AutofillPopupViewViews::Show() {
  DoShow();
  ui::AXPlatformNode::OnInputSuggestionsAvailable();
  // Fire these the first time a menu is visible. By firing these and the
  // matching end events, we are telling screen readers that the focus
  // is only changing temporarily, and the screen reader will restore the
  // focus back to the appropriate textfield when the menu closes.
  NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
}

void AutofillPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;
  ui::AXPlatformNode::OnInputSuggestionsUnavailable();
  DoHide();
  NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
}

void AutofillPopupViewViews::OnSuggestionsChanged() {
  // We recreate the child views so we can be sure the |controller_|'s
  // |GetLineCount()| will match the number of child views. Otherwise,
  // the number of suggestions i.e. |GetLineCount()| may not match 1x1 with the
  // child views. See crbug.com/697466.
  CreateChildViews();
  DoUpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  canvas->DrawColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ResultsTableNormalBackground));
  OnPaintBorder(canvas);

  DCHECK_EQ(controller_->GetLineCount(), child_count());
  for (int i = 0; i < controller_->GetLineCount(); ++i) {
    gfx::Rect line_rect = controller_->layout_model().GetRowBounds(i);

    if (controller_->GetSuggestionAt(i).frontend_id ==
        POPUP_ITEM_ID_SEPARATOR) {
      canvas->FillRect(line_rect,
                       GetNativeTheme()->GetSystemColor(
                           ui::NativeTheme::kColorId_ResultsTableDimmedText));
    } else {
      DrawAutofillEntry(canvas, i, line_rect);
    }
  }
}

void AutofillPopupViewViews::AddExtraInitParams(
    views::Widget::InitParams* params) {}

std::unique_ptr<views::View> AutofillPopupViewViews::CreateWrapperView() {
  auto wrapper_view = std::make_unique<views::ScrollView>();
  scroll_view_ = wrapper_view.get();
  scroll_view_->set_hide_horizontal_scrollbar(true);
  scroll_view_->SetContents(this);
  return wrapper_view;
}

std::unique_ptr<views::Border> AutofillPopupViewViews::CreateBorder() {
  return views::CreateSolidBorder(
      kPopupBorderThicknessDp,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_UnfocusedBorderColor));
}

void AutofillPopupViewViews::SetClipPath() {}

// The method differs from the implementation in AutofillPopupBaseView due to
// |scroll_view_|. The base class doesn't support scrolling when there is not
// enough vertical space.
void AutofillPopupViewViews::DoUpdateBoundsAndRedrawPopup() {
  gfx::Rect bounds = delegate()->popup_bounds();

  SetSize(bounds.size());

  gfx::Rect clipping_bounds = CalculateClippingBounds();

  int available_vertical_space = clipping_bounds.height() -
                                 (bounds.y() - clipping_bounds.y()) -
                                 kPopupBottomMargin;

  if (available_vertical_space < bounds.height()) {
    // The available space is not enough for the full popup so clamp the widget
    // to what's available. Since the scroll view will show a scroll bar,
    // increase the width so that the content isn't partially hidden.
    const int extra_width =
        scroll_view_ ? scroll_view_->GetScrollBarLayoutWidth() : 0;
    bounds.set_width(bounds.width() + extra_width);
    bounds.set_height(available_vertical_space);
  }

  // Account for the scroll view's border so that the content has enough space.
  bounds.Inset(-GetWidget()->GetRootView()->border()->GetInsets());
  GetWidget()->SetBounds(bounds);

  SchedulePaint();
}

AutofillPopupChildView* AutofillPopupViewViews::GetChildRow(
    size_t child_index) const {
  DCHECK_LT(child_index, static_cast<size_t>(child_count()));
  return static_cast<AutofillPopupChildView*>(
      const_cast<views::View*>(child_at(child_index)));
}

void AutofillPopupViewViews::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {
  SchedulePaint();

  if (previous_row_selection) {
    GetChildRow(*previous_row_selection)->OnUnselected();
  }
  if (current_row_selection) {
    AutofillPopupChildView* current_row = GetChildRow(*current_row_selection);
    current_row->OnSelected();
    current_row->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  }
}

/**
* Autofill entries in ltr.
*
* ............................................................................
* . ICON | HTTP WARNING MESSAGE VALUE                                | LABEL .
* ............................................................................
* . OTHER AUTOFILL ENTRY VALUE |                                LABEL | ICON .
* ............................................................................
*
* Autofill entries in rtl.
*
* ............................................................................
* . LABEL |                                HTTP WARNING MESSAGE VALUE | ICON .
* ............................................................................
* . ICON | LABEL                                | OTHER AUTOFILL ENTRY VALUE .
* ............................................................................
*
* Anyone who wants to modify the code below, remember to make sure that HTTP
* warning entry displays right. To trigger the warning message entry, enable
* #mark-non-secure-as flag as "display form warning", go to goo.gl/CEIjc6 with
* stored autofill info and check for credit card or password forms.
*/
void AutofillPopupViewViews::DrawAutofillEntry(gfx::Canvas* canvas,
                                               int index,
                                               const gfx::Rect& entry_rect) {
  canvas->FillRect(
      entry_rect,
      GetNativeTheme()->GetSystemColor(
          controller_->GetBackgroundColorIDForRow(index)));

  const bool is_rtl = controller_->IsRTL();
  const int text_align =
      is_rtl ? gfx::Canvas::TEXT_ALIGN_RIGHT : gfx::Canvas::TEXT_ALIGN_LEFT;
  gfx::Rect value_rect = entry_rect;
  value_rect.Inset(AutofillPopupLayoutModel::kEndPadding, 0);

  // If the icon is on the right of the rect, no matter in RTL or LTR mode.
  bool icon_on_the_right = !is_rtl;
  int x_align_left = icon_on_the_right ? value_rect.right() : value_rect.x();

  // Draw the Autofill icon, if one exists
  int row_height = controller_->layout_model().GetRowBounds(index).height();
  const gfx::ImageSkia image = controller_->layout_model().GetIconImage(index);
  if (!image.isNull()) {
    int icon_y = entry_rect.y() + (row_height - image.height()) / 2;

    int icon_x_align_left =
        icon_on_the_right ? x_align_left - image.width() : x_align_left;

    canvas->DrawImageInt(image, icon_x_align_left, icon_y);

    // An icon was drawn; adjust the |x_align_left| value for the next element.
      x_align_left =
          icon_x_align_left +
          (is_rtl ? image.width() + AutofillPopupLayoutModel::kIconPadding
                  : -AutofillPopupLayoutModel::kIconPadding);
  }

  // Draw the value text
  const int value_width = gfx::GetStringWidth(
      controller_->GetElidedValueAt(index),
      controller_->layout_model().GetValueFontListForRow(index));
  int value_x_align_left =
      is_rtl ? value_rect.right() - value_width : value_rect.x();

  canvas->DrawStringRectWithFlags(
      controller_->GetElidedValueAt(index),
      controller_->layout_model().GetValueFontListForRow(index),
      GetNativeTheme()->GetSystemColor(
          controller_->layout_model().GetValueFontColorIDForRow(index)),
      gfx::Rect(value_x_align_left, value_rect.y(), value_width,
                value_rect.height()),
      text_align);

  // Draw the label text, if one exists.
  if (!controller_->GetSuggestionAt(index).label.empty()) {
    const int label_width = gfx::GetStringWidth(
        controller_->GetElidedLabelAt(index),
        controller_->layout_model().GetLabelFontListForRow(index));
    int label_x_align_left = x_align_left + (is_rtl ? 0 : -label_width);

    // TODO(crbug.com/678033):Add a GetLabelFontColorForRow function similar to
    // GetValueFontColorForRow so that the cocoa impl could use it too
    canvas->DrawStringRectWithFlags(
        controller_->GetElidedLabelAt(index),
        controller_->layout_model().GetLabelFontListForRow(index),
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_ResultsTableDimmedText),
        gfx::Rect(label_x_align_left, entry_rect.y(), label_width,
                  entry_rect.height()),
        text_align);
  }
}

void AutofillPopupViewViews::CreateChildViews() {
  RemoveAllChildViews(true /* delete_children */);

  int set_size = controller_->GetLineCount();
  for (int i = 0; i < set_size; ++i) {
    AddChildView(new AutofillPopupChildView(controller_->GetSuggestionAt(i),
                                            set_size, i + 1));
  }
}

AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
#if defined(OS_MACOSX)
  // It's possible for the container_view to not be in a window. In that case,
  // cancel the popup since we can't fully set it up.
  if (!platform_util::GetTopLevel(controller->container_view()))
    return nullptr;
#endif

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

#if !defined(OS_MACOSX)
  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up. On Mac Cocoa browser, |observing_widget| is null
  // because the parent is not a views::Widget.
  if (!observing_widget)
    return nullptr;
#endif

  if (features::ShouldUseNativeViews())
    return new AutofillPopupViewNativeViews(controller, observing_widget);

  return new AutofillPopupViewViews(controller, observing_widget);
}

}  // namespace autofill
