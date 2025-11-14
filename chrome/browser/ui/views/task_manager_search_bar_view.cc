// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/task_manager_search_bar_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/branded_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace task_manager {
TaskManagerSearchBarView::TaskManagerSearchBarView(
    const std::u16string& placeholder,
    const gfx::Insets& margins,
    Delegate& delegate)
    : delegate_(delegate)
#if BUILDFLAG(IS_LINUX)
      ,
      textfield_placeholder_color_id_(kColorTaskManagerSearchBarPlaceholderText)
#endif
{
  auto* layout_provider = ChromeLayoutProvider::Get();
  auto search_bar_layout = std::make_unique<views::BoxLayout>();
  search_bar_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  search_bar_layout->set_main_axis_alignment(views::LayoutAlignment::kStretch);
  search_bar_layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  auto search_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kSearchChromeRefreshIcon, ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              DISTANCE_TASK_MANAGER_SEARCH_ICON_SIZE)));
  search_icon->SetProperty(views::kMarginsKey, margins);

  auto input =
      views::Builder<views::Textfield>()
          .SetPlaceholderText(placeholder)
          .SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_TASK_MANAGER_SEARCH_ACCESSIBILITY_NAME))
          .SetController(this)
          .SetBorder(nullptr)
          .SetBackgroundColor(kColorTaskManagerSearchBarBackground)
          .SetProperty(views::kElementIdentifierKey, kInputField)
          // Set margins to remove duplicate space between search
          // icon and textfield.
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 0))
          .Build();

  auto clear_btn =
      views::Builder<views::ImageButton>(
          views::CreateVectorImageButtonWithNativeTheme(
              base::BindRepeating(&TaskManagerSearchBarView::OnClearPressed,
                                  base::Unretained(this)),
              vector_icons::kCloseChromeRefreshIcon))
          // Reset the border set by
          // `CreateVectorImageButtonWithNativeTheme()` as it sets
          // an unnecessary padding to the highlighting circle.
          .SetBorder(nullptr)
          .SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_TASK_MANAGER_CLEAR_SEARCH_BUTTON_ACCESSIBILITY_NAME))
          .SetProperty(views::kMarginsKey, margins)
          .Build();

  search_bar_layout->SetFlexForView(input.get(), 1);

  SetLayoutManager(std::move(search_bar_layout));

  AddChildView(std::move(search_icon));
  input_ = AddChildView(std::move(input));
  input_->RemoveHoverEffect();
  input_changed_subscription_ =
      input_->AddTextChangedCallback(base::BindRepeating(
          &TaskManagerSearchBarView::OnInputChanged, base::Unretained(this)));

  clear_ = AddChildView(std::move(clear_btn));
  views::InstallCircleHighlightPathGenerator(clear_);
  // Only visible when users type in search keywords.
  if (input_->GetText().empty()) {
    clear_->SetVisible(false);
  }
}

TaskManagerSearchBarView::~TaskManagerSearchBarView() = default;

void TaskManagerSearchBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateTextfield();
}

bool TaskManagerSearchBarView::HandleKeyEvent(views::Textfield* /*sender*/,
                                              const ui::KeyEvent& key_event) {
  // Clear button should be visible only if input text is not empty.
  // Early return if visibility is consistent with the input text.
  // Case 1 : clear button is visible and input text is not empty.
  // Case 2 : clear button is invisible and input text is empty.
  if (clear_->GetVisible() != input_->GetText().empty()) {
    return false;
  }
  if (key_event.type() == ui::EventType::kKeyReleased) {
    clear_->SetVisible(!input_->GetText().empty());
  }
  return false;
}

void TaskManagerSearchBarView::Focus() {
  input_->RequestFocus();
}

void TaskManagerSearchBarView::OnInputChanged() {
  input_change_notification_timer_.Start(
      FROM_HERE, kInputChangeCallbackDelay,
      // `delegate_` is expected to outlive `this`, the timer will either be
      // triggered when it is alive or canceled.
      base::BindOnce(&Delegate::SearchBarOnInputChanged,
                     base::Unretained(delegate_), input_->GetText()));
}

void TaskManagerSearchBarView::OnClearPressed() {
  input_->SetText(u"");
  clear_->SetVisible(false);
  input_->RequestFocus();
}

bool TaskManagerSearchBarView::GetClearButtonVisibleStatusForTesting() const {
  return clear_->GetVisible();
}

void TaskManagerSearchBarView::SetInputTextForTesting(
    const std::u16string& text) {
  input_->SetText(text);
}

gfx::Point TaskManagerSearchBarView::GetClearButtonScreenCenterPointForTesting()
    const {
  return clear_->GetBoundsInScreen().CenterPoint();
}

void TaskManagerSearchBarView::UpdateTextfield() {
  if (const auto* const color_provider = GetColorProvider(); color_provider) {
    input_->set_placeholder_text_color(
        color_provider->GetColor(textfield_placeholder_color_id_.value_or(
            ui::kColorTextfieldForegroundPlaceholder)));
  }
}

BEGIN_METADATA(TaskManagerSearchBarView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TaskManagerSearchBarView, kInputField);
}  // namespace task_manager
