// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/plus_address_suggestion_helper.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/browser/plus_address_test_utils.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/common/features.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace plus_addresses {
namespace {

using autofill::Suggestion;
using autofill::SuggestionType;

class PlusAddressSuggestionHelperTest : public ::testing::Test {
 public:
  PlusAddressSuggestionHelperTest() = default;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_env_;
};

// Tests that suggestions are returned
TEST_F(PlusAddressSuggestionHelperTest, GetSuggestions) {
  PlusAddressSuggestionHelper generator;
  const std::string plus_address = "test+plus@test.com";

  EXPECT_THAT(generator.GetSuggestions(
                  /*affiliated_plus_addresses=*/{plus_address}),
              test::IsSingleFillPlusAddressSuggestion(plus_address));
}
}  // namespace
}  // namespace plus_addresses
