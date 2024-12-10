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
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace task_manager {
TaskManagerSearchBarView::TaskManagerSearchBarView(
    const std::u16string& placeholder,
    const gfx::Insets& margins) {
  auto* layout_provider = ChromeLayoutProvider::Get();

  auto search_bar_layout = std::make_unique<views::BoxLayout>();
  search_bar_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
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
  if (views::InkDrop::Get(input_)) {
    views::InkDrop::Get(input_)->SetBaseColorCallback(base::BindRepeating(
        [](views::Textfield* host) {
          const auto* color_provider = host->GetColorProvider();
          return color_provider && host->HasFocus()
                     ? color_provider->GetColor(
                           kColorTaskManagerSearchBarTransparent)
                     : color_provider->GetColor(
                           kColorTaskManagerSearchBarBackground);
        },
        input_));
  }
  clear_ = AddChildView(std::move(clear_btn));
  views::InstallCircleHighlightPathGenerator(clear_);
  // Only visible when users type in search keywords.
  if (input_->GetText().empty()) {
    clear_->SetVisible(false);
  }
}

TaskManagerSearchBarView::~TaskManagerSearchBarView() = default;

bool TaskManagerSearchBarView::HandleKeyEvent(views::Textfield* sender,
                                              const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyPressed &&
      !input_->GetText().empty() && !clear_->GetVisible()) {
    clear_->SetVisible(true);
  }
  return false;
}
void TaskManagerSearchBarView::Focus() {
  input_->RequestFocus();
}
void TaskManagerSearchBarView::OnClearPressed() {
  input_->SetText(u"");
  clear_->SetVisible(false);
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

BEGIN_METADATA(TaskManagerSearchBarView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TaskManagerSearchBarView, kInputField);
}  // namespace task_manager
