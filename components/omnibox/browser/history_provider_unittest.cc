// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/history_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestHistoryProvider : public HistoryProvider {
 public:
  explicit TestHistoryProvider(AutocompleteProviderClient* client)
      : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_QUICK, client) {}
  TestHistoryProvider(const TestHistoryProvider&) = delete;
  TestHistoryProvider& operator=(const TestHistoryProvider&) = delete;

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~TestHistoryProvider() override;
};

void TestHistoryProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {}

TestHistoryProvider::~TestHistoryProvider() = default;

class HistoryProviderTest : public testing::Test {
 public:
  HistoryProviderTest() = default;
  HistoryProviderTest(const HistoryProviderTest&) = delete;
  HistoryProviderTest& operator=(const HistoryProviderTest&) = delete;

 protected:
  void SetUp() override;
  void TearDown() override;

  FakeAutocompleteProviderClient* client() { return &(*client_); }
  HistoryProvider* provider() { return &(*provider_); }

 private:
  base::ScopedTempDir history_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<TestHistoryProvider> provider_;
};

void HistoryProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  CHECK(history_dir_.CreateUniqueTempDir());
  client_->set_history_service(
      history::CreateHistoryService(history_dir_.GetPath(), true));
  client_->set_bookmark_model(bookmarks::TestBookmarkClient::CreateModel());
  client_->set_in_memory_url_index(std::make_unique<InMemoryURLIndex>(
      client_->GetBookmarkModel(), client_->GetHistoryService(), nullptr,
      history_dir_.GetPath(), SchemeSet()));
  client_->GetInMemoryURLIndex()->Init();

  provider_ = new TestHistoryProvider(client_.get());
}

void HistoryProviderTest::TearDown() {
  provider_ = nullptr;
  client_.reset();
  task_environment_.RunUntilIdle();
}

// Placeholder test. Remove after adding a substantive test.
TEST_F(HistoryProviderTest, CreationTest) {
  EXPECT_NE(client(), nullptr);
}

}  // namespace
