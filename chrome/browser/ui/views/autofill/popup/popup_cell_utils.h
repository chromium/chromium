// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_UTILS_H_

#include <memory>
#include <optional>

#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/style/typography.h"

namespace views {
class View;
class Label;
class BoxLayout;
class ImageView;
}  // namespace views

namespace autofill {
class PopupRowContentView;
}  // namespace autofill

namespace gfx {
class Insets;
struct VectorIcon;
}

namespace autofill::popup_cell_utils {

// Returns the padding for a content cell.
//
// The following reasoning applies:
// * There is padding with distance `PopupRowView::GetHorizontalMargin()`
//   between the edge of  the Autofill popup row and the start of the content
//   cell.
// * In addition, there is also padding inside the content cell. Together, these
//   two paddings need to add up to `PopupBaseView::ArrowHorizontalMargin`,
//   since to ensure that the content inside the content cell is aligned with
//   the popup bubble's arrow.
//
//           / \
//          /   \
//         /     \
//        / arrow \
// ┌─────/         \────────────────────────┐
// │  ┌──────────────────────────────────┐  │
// │  │  ┌─────────┐ ┌────────────────┐  │  │
// ├──┼──┤         │ │                │  │  │
// ├──┤▲ │  Icon   │ │ Text labels    │  │  │
// │▲ │| │         │ │                │  │  │
// ││ ││ └─────────┘ └────────────────┘  │  │
// ││ └┼─────────────────────────────────┘  │
// └┼──┼────────────────────────────────────┘
//  │  │
//  │  PopupBaseView::ArrowHorizontalMargin()
//  │
//  PopupRowView::GetHorizontalMargin()
gfx::Insets GetMarginsForContentCell();

std::u16string GetVoiceOverStringFromSuggestion(const Suggestion& suggestion);

std::unique_ptr<views::ImageView> GetIconImageView(
    const Suggestion& suggestion);

std::unique_ptr<views::ImageView> GetTrailingIconImageView(
    const Suggestion& suggestion);

// Adds a spacer with `spacer_width` to `view`. `layout` must be the
// LayoutManager of `view`.
void AddSpacerWithSize(views::View& view,
                       views::BoxLayout& layout,
                       int spacer_width,
                       bool resize);

// Creates the table in which all the Autofill suggestion content apart from
// leading and trailing icons is contained and adds it to `content_view`.
// It registers `main_text_label`, `minor_text_label`, and `description_label`
// with `content_view` for tracking, but assumes that the labels inside of of
// `subtext_views` have already been registered for tracking with
// `content_view`.
void AddSuggestionContentTableToView(
    std::unique_ptr<views::Label> main_text_label,
    std::unique_ptr<views::Label> minor_text_label,
    std::unique_ptr<views::Label> description_label,
    std::vector<std::unique_ptr<views::View>> subtext_views,
    PopupRowContentView& content_view);

// Creates the content structure shared by autocomplete, address, credit card,
// and password suggestions.
// - `minor_text_label`, `description_label`, and `subtext_labels` may all be
// null or empty.
// - `content_view` is the (assumed to be empty) view to which the content
// structure for the `suggestion` is added.
void AddSuggestionContentToView(
    const Suggestion& suggestion,
    std::unique_ptr<views::Label> main_text_label,
    std::unique_ptr<views::Label> minor_text_label,
    std::unique_ptr<views::Label> description_label,
    std::vector<std::unique_ptr<views::View>> subtext_views,
    PopupRowContentView& content_view);

void FormatLabel(views::Label& label,
                 const Suggestion::Text& text,
                 FillingProduct main_filling_product,
                 int maximum_width_single_line);

// Creates a label for the suggestion's main text.
std::unique_ptr<views::Label> CreateMainTextLabel(const Suggestion& suggestion,
                                                  int text_style);

// Creates a label for the suggestion's minor text.
std::unique_ptr<views::Label> CreateMinorTextLabel(
    const Suggestion& suggestion);

// Creates sub-text views and pass their references to `PopupRowContentView` for
// centralized style management. If `text_style` is not provided, the default
// style from GetSecondaryTextStyle() will be used for the label views."
std::vector<std::unique_ptr<views::View>> CreateAndTrackSubtextViews(
    PopupRowContentView& content_view,
    const Suggestion& suggestion,
    FillingProduct main_filling_product,
    std::optional<int> text_style = std::nullopt);

int GetMaxPopupAddressProfileWidth();

std::unique_ptr<views::ImageView> ImageViewFromVectorIcon(
    const gfx::VectorIcon& vector_icon,
    int icon_size);

// Appplies a grayed-out disabled style to views conveying that it is
// deactivated and non-acceptable.
void ApplyDeactivatedStyle(views::View& view);

// Returns the expandable menu icon depending on `popup_item_id`.
const gfx::VectorIcon& GetExpandableMenuIcon(PopupItemId popup_item_id);

}  // namespace autofill::popup_cell_utils

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_UTILS_H_
