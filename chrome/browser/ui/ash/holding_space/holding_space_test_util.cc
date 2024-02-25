// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
GetSuggestionsInModel(const HoldingSpaceModel& model) {
  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
      model_suggestions;
  for (const auto& item : model.items()) {
    if (HoldingSpaceItem::IsSuggestionType(item->type())) {
      model_suggestions.emplace_back(item->type(), item->file().file_path);
    }
  }
  return model_suggestions;
}

void PressAndReleaseKey(ui::KeyboardCode key_code, int flags) {
  ui::test::EventGenerator(
      HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows())
      .PressAndReleaseKeyAndModifierKeys(key_code, flags);
}

void RightClick(const views::View* view, int flags) {
  ui::test::EventGenerator event_generator(
      view->GetWidget()->GetNativeWindow()->GetRootWindow());
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.PressModifierKeys(flags);
  event_generator.ClickRightButton();
  event_generator.ReleaseModifierKeys(flags);
}

views::MenuItemView* SelectMenuItemWithCommandId(
    HoldingSpaceCommandId command_id) {
  auto* const menu_controller = views::MenuController::GetActiveInstance();
  if (!menu_controller) {
    return nullptr;
  }

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  auto* const first_selected_menu_item = menu_controller->GetSelectedMenuItem();
  if (!first_selected_menu_item) {
    return nullptr;
  }

  auto* selected_menu_item = first_selected_menu_item;
  do {
    if (selected_menu_item->GetCommand() == static_cast<int>(command_id)) {
      return selected_menu_item;
    }

    PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
    selected_menu_item = menu_controller->GetSelectedMenuItem();

    // It is expected that context menus loop selection traversal. If the
    // currently `selected_menu_item` is the `first_selected_menu_item` then the
    // context menu has been completely traversed.
  } while (selected_menu_item != first_selected_menu_item);

  // If this LOC is reached the menu has been completely traversed without
  // finding a menu item for the desired `command_id`.
  return nullptr;
}

void WaitForItemRemoval(
    base::FunctionRef<bool(const HoldingSpaceItem*)> predicate) {
  auto* const model = HoldingSpaceController::Get()->model();
  if (base::ranges::none_of(model->items(), [&predicate](const auto& item) {
        return predicate(item.get());
      })) {
    return;
  }

  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(model);

  base::RunLoop run_loop;
  ON_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        for (const HoldingSpaceItem* item : items) {
          if (predicate(item)) {
            run_loop.Quit();
            return;
          }
        }
      });
  run_loop.Run();
}

void WaitForItemRemovalById(const std::string& item_id) {
  WaitForItemRemoval([&item_id](const HoldingSpaceItem* item) {
    return item->id() == item_id;
  });
}

void WaitForSuggestionsInModel(
    HoldingSpaceModel* model,
    const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
        expected_suggestions) {
  if (GetSuggestionsInModel(*model) == expected_suggestions)
    return;

  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(model);

  base::RunLoop run_loop;
  ON_CALL(mock, OnHoldingSpaceItemsAdded)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        if (GetSuggestionsInModel(*model) == expected_suggestions) {
          run_loop.Quit();
        }
      });
  ON_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        if (GetSuggestionsInModel(*model) == expected_suggestions) {
          run_loop.Quit();
        }
      });

  run_loop.Run();
}

}  // namespace ash
