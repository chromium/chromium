// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/annotation_reducer/autofill_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::accessibility_annotator::AutofillDataProvider;
using ::accessibility_annotator::MemorySearchResult;
using ::accessibility_annotator::QueryIntentType;

class FakeAutofillDataProvider : public AutofillDataProvider {
 public:
  std::vector<MemorySearchResult> RetrieveAll(QueryIntentType type) override {
    last_type_ = type;
    return results_;
  }
  void set_results(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }
  QueryIntentType last_type() const { return last_type_; }

 private:
  std::vector<MemorySearchResult> results_;
  QueryIntentType last_type_ = QueryIntentType::kUnknown;
};

class AccessibilityQueryServiceTest : public testing::Test {
 public:
  AccessibilityQueryServiceTest() = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AccessibilityQueryServiceTest, Query_Success) {
  auto data_provider = std::make_unique<FakeAutofillDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service =
      std::make_unique<AccessibilityQueryService>(std::move(data_provider));

  MemorySearchResult result;
  result.value = u"John Doe";
  result.title = u"John Doe";
  result.description = u"Name";
  fake_data_provider->set_results({result});

  auto results = service->Query(u"what is my name");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].value, u"John Doe");
  EXPECT_EQ(fake_data_provider->last_type(), QueryIntentType::kNameFull);
}

TEST_F(AccessibilityQueryServiceTest, Query_UnknownIntent) {
  auto data_provider = std::make_unique<FakeAutofillDataProvider>();
  auto service =
      std::make_unique<AccessibilityQueryService>(std::move(data_provider));

  auto results = service->Query(u"random query");
  EXPECT_TRUE(results.empty());
}

}  // namespace

}  // namespace accessibility_annotator
