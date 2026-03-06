// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

class MockFilterSuggestionGenerator : public FilterSuggestionGenerator {
 public:
  void GenerateSuggestion(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback)
      override {
    GenerateSuggestion(url);
    std::move(callback).Run(std::nullopt);
  }
  MOCK_METHOD(void, GenerateSuggestion, (const GURL&));
};

class MultistepFilterServiceTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(MultistepFilterServiceTest, CreateAndDestroy) {
  // Verifies the service can be created and destroyed without crashing.
  MultistepFilterService service(std::make_unique<FilterSuggestionGenerator>(),
                                 identity_test_env_.identity_manager());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  auto mock_generator = std::make_unique<MockFilterSuggestionGenerator>();
  MockFilterSuggestionGenerator* mock_generator_ptr = mock_generator.get();
  MultistepFilterService service(std::move(mock_generator),
                                 identity_test_env_.identity_manager());

  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_ptr, GenerateSuggestion(kUrl));

  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt)));

  service.GenerateFilterSuggestions(kUrl, callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NotSignedIn) {
  auto mock_generator = std::make_unique<MockFilterSuggestionGenerator>();
  MockFilterSuggestionGenerator* mock_generator_ptr = mock_generator.get();
  MultistepFilterService service(std::move(mock_generator),
                                 identity_test_env_.identity_manager());

  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_ptr, GenerateSuggestion(testing::_)).Times(0);

  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt)));

  service.GenerateFilterSuggestions(kUrl, callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NoGenerator) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  MultistepFilterService service(nullptr,
                                 identity_test_env_.identity_manager());

  const GURL kUrl("http://example.com");

  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt)));

  service.GenerateFilterSuggestions(kUrl, callback.Get());
}

}  // namespace multistep_filter
