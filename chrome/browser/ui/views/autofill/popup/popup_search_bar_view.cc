// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"

namespace autofill {

PopupSearchBarView::PopupSearchBarView(const std::u16string& placeholder,
                                       Delegate& delegate)
    : delegate_(delegate) {
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
          .SetPlaceholderText(placeholder)
          .SetController(this)
          .SetBorder(nullptr)
          .SetProperty(views::kElementIdentifierKey, kInputField)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(views::FlexSpecification(
                           views::LayoutOrientation::kHorizontal,
                           views::MinimumFlexSizeRule::kPreferred,
                           views::MaximumFlexSizeRule::kUnbounded)))
          .Build());

  input_changed_subscription_ =
      input_->AddTextChangedCallback(base::BindRepeating(
          &PopupSearchBarView::OnInputChanged, base::Unretained(this)));

  // TODO(crbug.com/325246516): Clarify whether the clear button should be
  // rendered on top of the input field and rework the layout (probably with a
  // custom LayoutManager).
  clear_ = AddChildView(
      views::Builder<views::ImageButton>()
          .SetCallback(base::BindRepeating(&PopupSearchBarView::OnClearPressed,
                                           base::Unretained(this)))
          .SetImageModel(views::Button::STATE_NORMAL,
                         ui::ImageModel::FromVectorIcon(
                             vector_icons::kCloseChromeRefreshIcon))
          .SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_POPUP_SEARCH_BAR_CLEAR_SEARCH_BUTTON_A11Y_NAME))
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
    delegate_->SearchBarOnFocusLost();
  }
}

bool PopupSearchBarView::HandleKeyEvent(views::Textfield* sender,
                                        const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyPressed) {
    return delegate_->SearchBarHandleKeyPressed(key_event);
  }
  return false;
}

void PopupSearchBarView::Focus() {
  input_->RequestFocus();
}

void PopupSearchBarView::SetInputTextForTesting(const std::u16string& text) {
  input_->SetText(text);
}

gfx::Point PopupSearchBarView::GetClearButtonScreenCenterPointForTesting()
    const {
  return clear_->GetBoundsInScreen().CenterPoint();
}

PopupSearchBarView::~PopupSearchBarView() = default;

void PopupSearchBarView::OnInputChanged() {
  input_change_notification_timer_.Start(
      FROM_HERE, kInputChangeCallbackDelay,
      // `delegate_` is expected to outlive `this`, the timer will either be
      // triggered when it is alive or canceled.
      base::BindOnce(&Delegate::SearchBarOnInputChanged,
                     base::Unretained(delegate_), input_->GetText()));
}

void PopupSearchBarView::OnClearPressed() {
  input_->SetText(u"");
}

BEGIN_METADATA(PopupSearchBarView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PopupSearchBarView, kInputField);

}  // namespace autofill
