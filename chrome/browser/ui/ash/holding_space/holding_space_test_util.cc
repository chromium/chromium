// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"

#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"

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
        if (GetSuggestionsInModel(*model) == expected_suggestions)
          run_loop.Quit();
      });
  ON_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        if (GetSuggestionsInModel(*model) == expected_suggestions)
          run_loop.Quit();
      });

  run_loop.Run();
}

}  // namespace ash
