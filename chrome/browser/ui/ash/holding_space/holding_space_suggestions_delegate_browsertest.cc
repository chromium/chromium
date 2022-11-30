// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/drive_file_suggestion_provider.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_test_util.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

// Returns the suggestion items in `model`.
std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
GetSuggestionsInModel(const HoldingSpaceModel& model) {
  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
      model_suggestions;
  for (const auto& item : model.items()) {
    if (HoldingSpaceItem::IsSuggestion(item->type()))
      model_suggestions.emplace_back(item->type(), item->file_path());
  }
  return model_suggestions;
}

// Waits until `expected_suggestions` appear in `model`.
void WaitForSuggestionsInModel(
    const testing::NiceMock<MockHoldingSpaceModelObserver>& mock,
    const HoldingSpaceModel& model,
    const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
        expected_suggestions) {
  if (GetSuggestionsInModel(model) == expected_suggestions)
    return;

  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsAdded)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        EXPECT_EQ(items.size(), expected_suggestions.size());
        std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
            actual_suggestions;
        for (const HoldingSpaceItem* item : items)
          actual_suggestions.emplace_back(item->type(), item->file_path());
        EXPECT_EQ(expected_suggestions, actual_suggestions);
        run_loop.Quit();
      });
  run_loop.Run();

  EXPECT_EQ(expected_suggestions, GetSuggestionsInModel(model));
}

}  // namespace

class HoldingSpaceSuggestionsDelegateBrowserTest
    : public drive::DriveIntegrationServiceBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  HoldingSpaceSuggestionsDelegateBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kHoldingSpaceSuggestions, GetParam());
  }

  // drive::DriveIntegrationServiceBrowserTestBase:
  void SetUpOnMainThread() override {
    drive::DriveIntegrationServiceBrowserTestBase::SetUpOnMainThread();
    app_list::WaitUntilFileSuggestServiceReady(GetFileSuggestKeyedService());
  }

  app_list::FileSuggestKeyedService* GetFileSuggestKeyedService() {
    return app_list::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
        browser()->profile());
  }

  void UpdateSuggestionsForDriveFiles(
      const std::vector<std::string>& file_ids) {
    std::vector<app_list::SuggestItemMetadata> update_params;
    for (const auto& file_id : file_ids)
      update_params.push_back({file_id, "display text", "prediction reason"});

    GetFileSuggestKeyedService()
        ->drive_file_suggestion_provider_for_test()
        ->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(
            app_list::CreateItemSuggestUpdateJsonString(update_params,
                                                        "session id"));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceSuggestionsDelegateBrowserTest,
                         /*enable_suggestion_feature=*/testing::Bool());

// Verifies that the holding space behaves as expected after the drive file
// suggestions update.
IN_PROC_BROWSER_TEST_P(HoldingSpaceSuggestionsDelegateBrowserTest,
                       OnDriveSuggestUpdate) {
  Profile* profile = browser()->profile();
  InitTestFileMountRoot(profile);

  // Add three drive files.
  const std::string file_id1("drive_file1");
  base::FilePath absolute_file_path1;
  AddDriveFileWithRelativePath(profile, file_id1, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path1);
  const std::string file_id2("drive_file2");
  base::FilePath absolute_file_path2;
  AddDriveFileWithRelativePath(profile, file_id2, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path2);
  const std::string file_id3("drive_file3");
  base::FilePath absolute_file_path3;
  AddDriveFileWithRelativePath(profile, file_id3, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path3);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> model_mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer{&model_mock};
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  model_observer.Observe(model);

  // Add an observer to watch for updates in drive file suggestions.
  testing::NiceMock<app_list::MockFileSuggestKeyedServiceObserver>
      service_observer_mock;
  base::ScopedObservation<app_list::FileSuggestKeyedService,
                          app_list::FileSuggestKeyedService::Observer>
      service_observer{&service_observer_mock};
  service_observer.Observe(GetFileSuggestKeyedService());

  UpdateSuggestionsForDriveFiles({file_id1, file_id2});
  app_list::WaitForFileSuggestionUpdate(
      service_observer_mock,
      /*expected_type=*/app_list::FileSuggestionType::kDriveFile);

  const bool holding_space_suggestion_enabled = GetParam();
  if (holding_space_suggestion_enabled) {
    // File 2 should be added to the model before file 1 so that the suggestion
    // of file 1 should show in front of the suggestion of file 2.
    std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
        expected_suggestions = {
            {HoldingSpaceItem::Type::kDriveSuggestion, absolute_file_path2},
            {HoldingSpaceItem::Type::kDriveSuggestion, absolute_file_path1}};
    WaitForSuggestionsInModel(model_mock, *model, expected_suggestions);
  } else {
    // Because `service_observer` starts observation after the holding space
    // suggestion delegate, `service_observer` should be notified of the file
    // suggestion update after the holding space. Therefore, it is safe to
    // check the model contents now.
    EXPECT_EQ(0u, model->items().size());
    // No item is added to the holding space model.
    EXPECT_CALL(model_mock, OnHoldingSpaceItemsAdded).Times(0);
    // There should be no client fetching file suggestions.
    EXPECT_FALSE(
        GetFileSuggestKeyedService()->HasPendingSuggestionFetchForTest());
  }

  UpdateSuggestionsForDriveFiles({file_id2, file_id3});
  app_list::WaitForFileSuggestionUpdate(
      service_observer_mock,
      /*expected_type=*/app_list::FileSuggestionType::kDriveFile);

  if (holding_space_suggestion_enabled) {
    // File 3 should be added to the model before file 2 so that the suggestion
    // of file 2 should show in front of the suggestion of file 3.
    std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
        expected_suggestions = {
            {HoldingSpaceItem::Type::kDriveSuggestion, absolute_file_path3},
            {HoldingSpaceItem::Type::kDriveSuggestion, absolute_file_path2}};
    WaitForSuggestionsInModel(model_mock, *model, expected_suggestions);
  } else {
    EXPECT_EQ(0u, model->items().size());
    // No item is added to the holding space model.
    EXPECT_CALL(model_mock, OnHoldingSpaceItemsAdded).Times(0);
    // There should be no client fetching file suggestions.
    EXPECT_FALSE(
        GetFileSuggestKeyedService()->HasPendingSuggestionFetchForTest());
  }
}

}  // namespace ash
