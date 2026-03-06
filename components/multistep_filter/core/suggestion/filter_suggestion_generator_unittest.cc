// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

class FilterSuggestionGeneratorTest : public testing::Test {
 public:
  FilterSuggestionGeneratorTest() = default;
  ~FilterSuggestionGeneratorTest() override = default;

 protected:
  FilterSuggestionGenerator generator_;
};

TEST_F(FilterSuggestionGeneratorTest, CreateAndDestroy) {
  // Verifies the service can be created and destroyed without crashing.
  EXPECT_FALSE(HasFatalFailure());
}

TEST_F(FilterSuggestionGeneratorTest, GenerateSuggestion) {
  // Since implementation is NOTIMPLEMENTED, we just verify it can be called.
  // We use a MockCallback to verify the interface.
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      callback;

  // We expect the callback to be run with std::nullopt.
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt)));

  const GURL url("http://www.google.com");
  generator_.GenerateSuggestion(url, callback.Get());
}

}  // namespace multistep_filter
