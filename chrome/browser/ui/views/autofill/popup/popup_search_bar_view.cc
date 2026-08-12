// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

// Custom textfield for the search input in `PopupSearchBarView`.
class SearchBarTextfield : public views::Textfield {
  METADATA_HEADER(SearchBarTextfield, views::Textfield)
 public:
  SearchBarTextfield(const std::u16string& placeholder,
                     const std::u16string& initial_value,
                     views::TextfieldController* controller) {
    SetPlaceholderText(placeholder);
    if (!initial_value.empty()) {
      SetText(initial_value);
    }
    SetAccessibleName(placeholder);
    SetController(controller);
    SetBorder(nullptr);
    SetProperty(views::kElementIdentifierKey, PopupSearchBarView::kInputField);
    SetProperty(views::kFlexBehaviorKey,
                views::FlexSpecification(views::FlexSpecification(
                    views::LayoutOrientation::kHorizontal,
                    views::MinimumFlexSizeRule::kPreferred,
                    views::MaximumFlexSizeRule::kUnbounded)));
  }

  ~SearchBarTextfield() override = default;

  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_TAB) {
      return true;
    }
    return views::Textfield::SkipDefaultKeyEventProcessing(event);
  }
};

BEGIN_METADATA(SearchBarTextfield)
END_METADATA

// Custom image button to clear the text input in `PopupSearchBarView`.
class SearchBarClearButton : public views::ImageButton {
  METADATA_HEADER(SearchBarClearButton, views::ImageButton)
 public:
  SearchBarClearButton(PressedCallback callback,
                       PopupSearchBarView* search_bar_view,
                       bool visible)
      : views::ImageButton(std::move(callback)),
        search_bar_view_(*search_bar_view) {
    views::ConfigureVectorImageButton(this);
    views::SetImageFromVectorIconWithColor(
        this,
        ::features::IsRoundedIconsEnabled()
            ? vector_icons::kCloseIcon
            : vector_icons::kCloseChromeRefreshOldIcon,
        views::IconColors(ui::kColorIcon, ui::kColorIconDisabled));
    SetBorder(nullptr);
    SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_POPUP_SEARCH_BAR_CLEAR_SEARCH_BUTTON_A11Y_NAME));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InstallCircleHighlightPathGenerator(this);
    SetVisible(visible);
  }

  ~SearchBarClearButton() override = default;

  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_TAB) {
      return true;
    }
    return views::ImageButton::SkipDefaultKeyEventProcessing(event);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (search_bar_view_->HandleKeyPressed(this, event)) {
      return true;
    }
    return views::ImageButton::OnKeyPressed(event);
  }

 private:
  const raw_ref<PopupSearchBarView> search_bar_view_;
};

BEGIN_METADATA(SearchBarClearButton)
END_METADATA

}  // namespace

PopupSearchBarView::PopupSearchBarView(const std::u16string& placeholder,
                                       const std::u16string& initial_value,
                                       Delegate& delegate,
                                       bool show_indicator,
                                       bool show_search_icon_sparkle,
                                       base::TimeDelta debounce_delay)
    : delegate_(delegate), debounce_delay_(debounce_delay) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                                 views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  int icon_size = layout_provider->GetDistanceMetric(
      views::DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE);

  const gfx::VectorIcon* icon = nullptr;
  if (show_search_icon_sparkle) {
    icon = &omnibox::kSearchSparkIcon;
  } else {
    icon = features::IsRoundedIconsEnabled()
               ? &vector_icons::kSearchIcon
               : &vector_icons::kSearchChromeRefreshOldIcon;
  }

  search_icon_ = AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(*icon, ui::kColorIcon, icon_size)));
  throbber_ = AddChildView(std::make_unique<views::Throbber>(icon_size));
  SetLoading(false);

  input_ = AddChildView(
      std::make_unique<SearchBarTextfield>(placeholder, initial_value, this));

  input_changed_subscription_ =
      input_->AddTextChangedCallback(base::BindRepeating(
          &PopupSearchBarView::OnInputChanged, base::Unretained(this)));

  // TODO(crbug.com/325246516): Clarify whether the clear button should be
  // rendered on top of the input field and rework the layout (probably with a
  // custom LayoutManager).
  clear_ = AddChildView(std::make_unique<SearchBarClearButton>(
      base::BindRepeating(&PopupSearchBarView::OnClearPressed,
                          base::Unretained(this)),
      this,
      /*visible=*/!initial_value.empty()));

  if (show_indicator) {
    indicator_ = AddChildView(views::Builder<views::Label>()
                                  .SetText(u"@@")
                                  .SetAutoColorReadabilityEnabled(false)
                                  .Build());
    indicator_->SetEnabledColor(ui::kColorTextfieldForegroundPlaceholder);
    indicator_->SetVisible(initial_value.empty());
  }
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

bool PopupSearchBarView::HandleKeyPressed(views::View* sender,
                                          const ui::KeyEvent& event) {
  if (event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  if (sender == input_) {
    if (event.key_code() == ui::VKEY_RETURN) {
      input_change_notification_timer_.Stop();
    }
    if (delegate_->SearchBarHandleKeyPressed(event)) {
      return true;
    }
    if (event.key_code() == ui::VKEY_TAB) {
      if (clear_->GetVisible()) {
        clear_->RequestFocus();
      } else {
        input_->RequestFocus();
      }
      return true;
    }
  } else if (sender == clear_) {
    if (event.key_code() == ui::VKEY_TAB) {
      input_->RequestFocus();
      return true;
    }
    if (delegate_->SearchBarHandleKeyPressed(event)) {
      return true;
    }
  }
  return false;
}

bool PopupSearchBarView::HandleKeyEvent(views::Textfield* sender,
                                        const ui::KeyEvent& key_event) {
  return HandleKeyPressed(sender, key_event);
}

void PopupSearchBarView::SetLoading(bool is_loading) {
  search_icon_->SetVisible(!is_loading);
  throbber_->SetVisible(is_loading);
  if (is_loading) {
    throbber_->Start();
  } else {
    throbber_->Stop();
  }
}

void PopupSearchBarView::Focus() {
  input_->RequestFocus();
}

std::u16string PopupSearchBarView::GetText() const {
  return input_ ? std::u16string(input_->GetText()) : std::u16string();
}

void PopupSearchBarView::SetInputTextForTesting(const std::u16string& text) {
  input_->SetText(text);
}

gfx::Point PopupSearchBarView::GetClearButtonScreenCenterPointForTesting()
    const {
  return clear_->GetBoundsInScreen().CenterPoint();
}

bool PopupSearchBarView::IsClearButtonVisibleForTesting() const {
  return clear_->GetVisible();
}

bool PopupSearchBarView::IsIndicatorVisibleForTesting() const {
  return indicator_ ? indicator_->GetVisible() : false;
}

PopupSearchBarView::~PopupSearchBarView() = default;

void PopupSearchBarView::OnInputChanged() {
  bool empty = input_->GetText().empty();
  clear_->SetVisible(!empty);
  if (empty && clear_->HasFocus()) {
    input_->RequestFocus();
  }
  if (indicator_) {
    indicator_->SetVisible(empty);
  }
  input_change_notification_timer_.Start(
      FROM_HERE, debounce_delay_,
      // `delegate_` is expected to outlive `this`, the timer will either be
      // triggered when it is alive or canceled.
      base::BindOnce(&Delegate::SearchBarOnInputChanged,
                     base::Unretained(delegate_), input_->GetText()));
}

void PopupSearchBarView::OnClearPressed() {
  input_->SetText({});
  input_->RequestFocus();
}

BEGIN_METADATA(PopupSearchBarView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PopupSearchBarView, kInputField);

}  // namespace autofill
