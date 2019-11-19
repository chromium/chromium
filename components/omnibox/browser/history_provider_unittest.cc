// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/history_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestHistoryProvider : public HistoryProvider {
 public:
  explicit TestHistoryProvider(AutocompleteProviderClient* client)
      : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_QUICK, client) {}

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~TestHistoryProvider() override;

  DISALLOW_COPY_AND_ASSIGN(TestHistoryProvider);
};

void TestHistoryProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {}

TestHistoryProvider::~TestHistoryProvider() {}

class HistoryProviderTest : public testing::Test {
 public:
  HistoryProviderTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  FakeAutocompleteProviderClient* client() { return &(*client_); }
  HistoryProvider* provider() { return &(*provider_); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<TestHistoryProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(HistoryProviderTest);
};

void HistoryProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();
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
