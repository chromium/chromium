// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"

namespace autofill {

PopupSearchBarView::PopupSearchBarView(
    const std::u16string& placeholder,
    base::RepeatingClosure on_focus_lost_callback)
    : on_focus_lost_callback_(std::move(on_focus_lost_callback)) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                                 views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kSearchChromeRefreshIcon, ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE))));

  input_ = AddChildView(
      views::Builder<views::Textfield>()
          // TODO(b/325246516): Set default placeholder according to approved
          // greenlines.
          .SetPlaceholderText(placeholder.empty() ? u"Search" : placeholder)
          .SetBorder(nullptr)
          .SetProperty(views::kElementIdentifierKey, kInputField)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(views::FlexSpecification(
                           views::LayoutOrientation::kHorizontal,
                           views::MinimumFlexSizeRule::kPreferred,
                           views::MaximumFlexSizeRule::kUnbounded)))
          .Build());

  // TODO(b/325246516): Clarify whether the clear button should be rendered
  // on top of the input field and rework the layout (probably with a custom
  // LayoutManager).
  clear_ = AddChildView(
      views::Builder<views::ImageButton>()
          .SetImageModel(views::Button::STATE_NORMAL,
                         ui::ImageModel::FromVectorIcon(
                             vector_icons::kCloseChromeRefreshIcon))
          // TODO(b/325246516): Set the name according to approved greenlines.
          .SetAccessibleName(u"tmp non empty name")
          .Build());
}

void PopupSearchBarView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void PopupSearchBarView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void PopupSearchBarView::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  if (focused_now != input_ && focused_now != clear_) {
    on_focus_lost_callback_.Run();
  }
}

void PopupSearchBarView::Focus() {
  input_->RequestFocus();
}

PopupSearchBarView::~PopupSearchBarView() = default;

BEGIN_METADATA(PopupSearchBarView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PopupSearchBarView, kInputField);

}  // namespace autofill
