// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/search/search_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/fake_hierarchy.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_sections.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kPrintingDetailsSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const bool kIsRevampEnabled =
    ash::features::IsOsSettingsRevampWayfindingEnabled();

class FakeObserver : public mojom::SearchResultsObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  mojo::PendingRemote<mojom::SearchResultsObserver> GenerateRemote() {
    mojo::PendingRemote<mojom::SearchResultsObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  size_t num_calls() const { return num_calls_; }

 private:
  // mojom::SearchResultsObserver:
  void OnSearchResultsChanged() override { ++num_calls_; }

  size_t num_calls_ = 0;
  mojo::Receiver<mojom::SearchResultsObserver> receiver_{this};
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
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPrintingDetails},
       {IDS_OS_SETTINGS_TAG_PRINTING_ALT1, IDS_OS_SETTINGS_TAG_PRINTING_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

// Creates a result with some default values.
mojom::SearchResultPtr CreateDummyResult() {
  const mojom::Section kSection = kIsRevampEnabled
                                      ? mojom::Section::kSystemPreferences
                                      : mojom::Section::kPrinting;

  return mojom::SearchResult::New(
      /*text=*/std::u16string(),
      /*canonical_text=*/std::u16string(), /*url=*/"",
      mojom::SearchResultIcon::kPrinter, /*relevance_score=*/0.5,
      /*hierarchy_strings=*/std::vector<std::u16string>(),
      mojom::SearchResultDefaultRank::kMedium,
      /*was_generated_from_text_match=*/false,
      mojom::SearchResultType::kSection,
      mojom::SearchResultIdentifier::NewSection(kSection));
}

}  // namespace

class SearchHandlerTest : public testing::Test {
 protected:
  SearchHandlerTest()
      : search_tag_registry_(local_search_service_proxy_.get()),
        fake_hierarchy_(&fake_sections_),
        handler_(&search_tag_registry_,
                 &fake_sections_,
                 &fake_hierarchy_,
                 local_search_service_proxy_.get()) {}
  ~SearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    const mojom::Section kSection = kIsRevampEnabled
                                        ? mojom::Section::kSystemPreferences
                                        : mojom::Section::kPrinting;

    fake_hierarchy_.AddSubpageMetadata(
        IDS_SETTINGS_PRINTING_CUPS_PRINTERS, kSection,
        mojom::Subpage::kPrintingDetails, mojom::SearchResultIcon::kPrinter,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::kPrintingDetailsSubpagePath);
    fake_hierarchy_.AddSettingMetadata(kSection, mojom::Setting::kAddPrinter);
    fake_hierarchy_.AddSettingMetadata(kSection,
                                       mojom::Setting::kSavedPrinters);

    handler_remote_->Observe(observer_.GenerateRemote());
    handler_remote_.FlushForTesting();
  }

  void AddSearchTags(const std::vector<SearchConcept>& search_tags) {
    SearchTagRegistry::ScopedTagUpdater updater =
        search_tag_registry_.StartUpdate();
    updater.AddSearchTags(search_tags);
  }

  void RemoveSearchTags(const std::vector<SearchConcept>& search_tags) {
    SearchTagRegistry::ScopedTagUpdater updater =
        search_tag_registry_.StartUpdate();
    updater.RemoveSearchTags(search_tags);
  }

  std::vector<mojom::SearchResultPtr> Search(
      const std::u16string& query,
      uint32_t max_num_results,
      mojom::ParentResultBehavior parent_result_behavior) {
    base::test::TestFuture<std::vector<mojom::SearchResultPtr>> future;
    handler_remote_->Search(query, max_num_results, parent_result_behavior,
                            future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchTagRegistry search_tag_registry_;
  FakeOsSettingsSections fake_sections_;
  FakeHierarchy fake_hierarchy_;
  SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
  FakeObserver observer_;
};

TEST_F(SearchHandlerTest, AddAndRemove) {
  // Add printing search tags to registry and search for "Print".
  AddSearchTags(GetPrintingSearchConcepts());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // 3 results should be available for a "Print" query.
  search_results =
      Search(u"Print",
             /*max_num_results=*/3u,
             mojom::ParentResultBehavior::kDoNotIncludeParentResults);
  EXPECT_EQ(search_results.size(), 3u);

  // Limit results to 1 max and ensure that only 1 result is returned.
  search_results =
      Search(u"Print",
             /*max_num_results=*/1u,
             mojom::ParentResultBehavior::kDoNotIncludeParentResults);
  EXPECT_EQ(search_results.size(), 1u);

  // Search for a query which should return no results.
  search_results =
      Search(u"QueryWithNoResults",
             /*max_num_results=*/3u,
             mojom::ParentResultBehavior::kDoNotIncludeParentResults);
  EXPECT_TRUE(search_results.empty());

  // Remove printing search tags to registry and verify that no results are
  // returned for "Printing".
  RemoveSearchTags(GetPrintingSearchConcepts());
  search_results =
      Search(u"Print",
             /*max_num_results=*/3u,
             mojom::ParentResultBehavior::kDoNotIncludeParentResults);
  EXPECT_TRUE(search_results.empty());
  EXPECT_EQ(2u, observer_.num_calls());
}

TEST_F(SearchHandlerTest, UrlModification) {
  // Add printing search tags to registry and search for "Saved".
  AddSearchTags(GetPrintingSearchConcepts());
  std::vector<mojom::SearchResultPtr> search_results =
      Search(u"Saved",
             /*max_num_results=*/3u,
             mojom::ParentResultBehavior::kDoNotIncludeParentResults);

  // Only the "saved printers" item should be returned.
  EXPECT_EQ(search_results.size(), 1u);

  // The URL should have bee modified according to the FakeOsSettingSection
  // scheme.
  const std::string kPrefix = kIsRevampEnabled
                                  ? std::string("kSystemPreferences::")
                                  : std::string("kPrinting::");

  EXPECT_EQ(kPrefix + mojom::kPrintingDetailsSubpagePath,
            search_results[0]->url_path_with_parameters);
}

TEST_F(SearchHandlerTest, AltTagMatch) {
  // Add printing search tags to registry.
  AddSearchTags(GetPrintingSearchConcepts());

  // Search for "CUPS". The IDS_OS_SETTINGS_TAG_PRINTING result has an alternate
  // tag "CUPS" (referring to the Unix printing protocol), so we should receive
  // one match.
  std::vector<mojom::SearchResultPtr> search_results =
      Search(u"CUPS",
             /*max_num_results=*/3u,
             mojom::ParentResultBehavior::kDoNotIncludeParentResults);
  EXPECT_EQ(search_results.size(), 1u);

  // Verify the result text and canonical restult text.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OS_SETTINGS_TAG_PRINTING_ALT2),
            search_results[0]->text);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OS_SETTINGS_TAG_PRINTING),
            search_results[0]->canonical_text);
}

TEST_F(SearchHandlerTest, AllowParentResult) {
  // Add printing search tags to registry.
  AddSearchTags(GetPrintingSearchConcepts());

  // Search for "Saved", which should only apply to the "saved printers" item.
  // Pass the kAllowParentResults flag, which should also cause its parent
  // subpage item to be returned.
  std::vector<mojom::SearchResultPtr> search_results = Search(
      u"Saved",
      /*max_num_results=*/3u, mojom::ParentResultBehavior::kAllowParentResults);
  EXPECT_EQ(search_results.size(), 2u);
  EXPECT_FALSE(search_results[1]->was_generated_from_text_match);
}

TEST_F(SearchHandlerTest, DefaultRank) {
  // Add printing search tags to registry.
  AddSearchTags(GetPrintingSearchConcepts());

  // Search for "Print". Only the IDS_OS_SETTINGS_TAG_PRINTING result
  // contains the word "Printing", but the other results have the similar word
  // "Printer". Thus, "Printing" has a higher relevance score.
  std::vector<mojom::SearchResultPtr> search_results = Search(
      u"Print",
      /*max_num_results=*/3u, mojom::ParentResultBehavior::kAllowParentResults);
  EXPECT_EQ(search_results.size(), 3u);

  // Since the IDS_OS_SETTINGS_TAG_PRINTING result has a default rank of kLow,
  // it should be the *last* result returned even though it has a higher
  // relevance score.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OS_SETTINGS_TAG_PRINTING),
            search_results[2]->text);
}

// Regression test for https://crbug.com/1090184.
TEST_F(SearchHandlerTest, CompareSearchResults) {
  // Create two equal dummy results.
  mojom::SearchResultPtr a = CreateDummyResult();
  mojom::SearchResultPtr b = CreateDummyResult();

  // CompareSearchResults() returns whether |a| < |b|; since they are equal, it
  // should return false regardless of the order of parameters.
  EXPECT_FALSE(SearchHandler::CompareSearchResults(a, b));
  EXPECT_FALSE(SearchHandler::CompareSearchResults(b, a));

  // Differ only on default rank.
  a = CreateDummyResult();
  a->default_rank = mojom::SearchResultDefaultRank::kLow;
  b = CreateDummyResult();
  b->default_rank = mojom::SearchResultDefaultRank::kHigh;

  // Comparison value should differ.
  EXPECT_NE(SearchHandler::CompareSearchResults(b, a),
            SearchHandler::CompareSearchResults(a, b));

  // Differ only on relevance score.
  a = CreateDummyResult();
  a->relevance_score = 0;
  b = CreateDummyResult();
  b->relevance_score = 1;

  // Comparison value should differ.
  EXPECT_NE(SearchHandler::CompareSearchResults(b, a),
            SearchHandler::CompareSearchResults(a, b));

  // Differ only on type.
  a = CreateDummyResult();
  a->type = mojom::SearchResultType::kSection;
  b = CreateDummyResult();
  b->type = mojom::SearchResultType::kSubpage;

  // Comparison value should differ.
  EXPECT_NE(SearchHandler::CompareSearchResults(b, a),
            SearchHandler::CompareSearchResults(a, b));
}

}  // namespace ash::settings
