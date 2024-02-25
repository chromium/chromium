// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_concept.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kPrintingDetailsSubpagePath;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

class FakeObserver : public SearchTagRegistry::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

 private:
  // SearchTagRegistry::Observer:
  void OnRegistryUpdated() override { ++num_calls_; }

  size_t num_calls_ = 0;
};

// Note: Copied from printing_section.cc but does not need to stay in sync with
// it.
const std::vector<SearchConcept>& GetPrintingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddPrinter}},
      {IDS_OS_SETTINGS_TAG_PRINTING_SAVED_PRINTERS,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSavedPrinters}},
      {IDS_OS_SETTINGS_TAG_PRINTING,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPrintingDetails},
       {IDS_OS_SETTINGS_TAG_PRINTING_ALT1, IDS_OS_SETTINGS_TAG_PRINTING_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

class SearchTagRegistryTest : public testing::Test {
 protected:
  SearchTagRegistryTest()
      : search_tag_registry_(local_search_service_proxy_.get()) {}

  ~SearchTagRegistryTest() override = default;

  // testing::Test:
  void SetUp() override {
    search_tag_registry_.AddObserver(&observer_);

    local_search_service_proxy_->GetIndex(
        local_search_service::IndexId::kCrosSettings,
        local_search_service::Backend::kLinearMap,
        index_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { search_tag_registry_.RemoveObserver(&observer_); }

  void IndexGetSizeAndCheckResults(uint32_t expected_num_items) {
    bool callback_done = false;
    uint32_t num_items = 0;
    index_remote_->GetSize(base::BindOnce(
        [](bool* callback_done, uint32_t* num_items, uint64_t size) {
          *callback_done = true;
          *num_items = size;
        },
        &callback_done, &num_items));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(num_items, expected_num_items);
  }

  // This line should be before search_tag_registry_ is declared.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchTagRegistry search_tag_registry_;
  FakeObserver observer_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
};

TEST_F(SearchTagRegistryTest, AddAndRemove) {
  // Add search tags; size of the index should increase.
  {
    SearchTagRegistry::ScopedTagUpdater updater =
        search_tag_registry_.StartUpdate();
    updater.AddSearchTags(GetPrintingSearchConcepts());

    // Nothing should have happened yet, since |updater| has not gone out of
    // scope.
    IndexGetSizeAndCheckResults(0u);
    EXPECT_EQ(0u, observer_.num_calls());
  }
  // Now that it went out of scope, the update should have occurred.
  IndexGetSizeAndCheckResults(3u);
  EXPECT_EQ(1u, observer_.num_calls());

  std::string first_tag_id =
      SearchTagRegistry::ToResultId(GetPrintingSearchConcepts()[0]);

  // Tags added should be available via GetTagMetadata().
  const SearchConcept* add_printer_concept =
      search_tag_registry_.GetTagMetadata(first_tag_id);
  ASSERT_TRUE(add_printer_concept);
  EXPECT_EQ(mojom::Setting::kAddPrinter, add_printer_concept->id.setting);

  // Remove search tag; size should go back to 0.
  {
    SearchTagRegistry::ScopedTagUpdater updater =
        search_tag_registry_.StartUpdate();
    updater.RemoveSearchTags(GetPrintingSearchConcepts());

    // Tags should not have been removed yet, since |updater| has not gone out
    // of scope.
    IndexGetSizeAndCheckResults(3u);
    EXPECT_EQ(1u, observer_.num_calls());
  }
  // Now that it went out of scope, the update should have occurred.
  IndexGetSizeAndCheckResults(0u);
  EXPECT_EQ(2u, observer_.num_calls());

  // The tag should no longer be accessible via GetTagMetadata().
  add_printer_concept = search_tag_registry_.GetTagMetadata(first_tag_id);
  ASSERT_FALSE(add_printer_concept);
}

}  // namespace ash::settings
