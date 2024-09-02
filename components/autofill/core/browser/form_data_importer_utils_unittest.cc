// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer_utils.h"

#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/form_structure.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// As TimestampedSameOriginQueue cannot be initialized with primitive types,
// this wrapper is used for testing.
struct IntWrapper {
  int value;
};
bool operator==(IntWrapper x, int y) {
  return x.value == y;
}

class FormDataImporterUtilsTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// TimestampedSameOriginQueue's queue-like functionality works as expected.
TEST_F(FormDataImporterUtilsTest, TimestampedSameOriginQueue) {
  TimestampedSameOriginQueue<IntWrapper> queue;
  EXPECT_TRUE(queue.empty());
  const url::Origin irrelevant_origin;
  for (int i = 0; i < 4; i++)
    queue.Push({i}, irrelevant_origin);
  EXPECT_EQ(queue.size(), 4u);
  EXPECT_THAT(queue, testing::ElementsAre(3, 2, 1, 0));
  queue.Pop();
  EXPECT_THAT(queue, testing::ElementsAre(3, 2, 1));
  queue.erase(std::next(queue.begin()), queue.end());
  EXPECT_THAT(queue, testing::ElementsAre(3));
  queue.Clear();
  EXPECT_TRUE(queue.empty());
}

TEST_F(FormDataImporterUtilsTest, TimestampedSameOriginQueue_MaxSize) {
  TimestampedSameOriginQueue<IntWrapper> queue{/*max_size=*/1};
  const url::Origin irrelevant_origin;
  queue.Push({0}, irrelevant_origin);
  queue.Push({1}, irrelevant_origin);
  EXPECT_THAT(queue, testing::ElementsAre(1));
}

// RemoveOutdatedItems clears the queue if the origin doesn't match.
TEST_F(FormDataImporterUtilsTest, TimestampedSameOriginQueue_DifferentOrigins) {
  TimestampedSameOriginQueue<IntWrapper> queue;
  auto foo_origin = url::Origin::Create(GURL("http://foo.com"));
  queue.Push({0}, foo_origin);
  EXPECT_EQ(queue.origin(), foo_origin);
  // The TTL or 1 hour is irrelevant here.
  queue.RemoveOutdatedItems(base::Hours(1),
                            url::Origin::Create(GURL("http://bar.com")));
  EXPECT_EQ(queue.origin(), std::nullopt);
  EXPECT_TRUE(queue.empty());
}

// RemoveOutdatedItems clears items past their TTL.
TEST_F(FormDataImporterUtilsTest, TimestampedSameOriginQueue_TTL) {
  TimestampedSameOriginQueue<IntWrapper> queue;
  const url::Origin irrelevant_origin;
  for (int i = 0; i < 4; i++) {
    queue.Push({i}, irrelevant_origin);
    task_environment_.FastForwardBy(base::Minutes(1));
  }
  // Remove all items older than 2.5 min.
  queue.RemoveOutdatedItems(base::Seconds(150), irrelevant_origin);
  EXPECT_THAT(queue, testing::ElementsAre(3, 2));
}

// Each test describes a sequence of submitted forms, where 'a' and 'c' indicate
// an address and a credit card form, respectively.
// Using an upper case A or C, forms that are supposed to be part of the
// association are marked.
constexpr std::string_view kFormAssociatorTestCases[]{
    // A single address/credit card form is associated with itself.
    "A",
    "C",
    // The credit card form is associated with the address form, and vice-versa.
    "AC",
    "CA",
    // Two address forms are associated with the credit card form.
    "AAC",
    // The first address form is not associated to the credit card form.
    "aAAC",
    // Only the last credit card form is kept.
    "AAcC",
};

class FormAssociatorTest : public testing::TestWithParam<std::string_view> {};

INSTANTIATE_TEST_SUITE_P(FormDataImporterUtilsTest,
                         FormAssociatorTest,
                         testing::ValuesIn(kFormAssociatorTestCases));

// Tests that all `kFormAssociationTestCases` yield the correct associations.
TEST_P(FormAssociatorTest, FormAssociator) {
  FormAssociator form_associator;
  url::Origin irrelevant_origin;
  FormStructure::FormAssociations expected_associations;
  const std::string_view& test = GetParam();
  // Each test verifies the association of the last form. If the last form is
  // not expected to be included, that's likely a typo.
  EXPECT_TRUE(!test.empty() && base::IsAsciiUpper(test.back()));

  for (size_t i = 0; i < test.size(); i++) {
    FormSignature signature{i};
    auto type = base::ToLowerASCII(test[i]) == 'a'
                    ? FormAssociator::FormType::kAddressForm
                    : FormAssociator::FormType::kCreditCardForm;
    form_associator.TrackFormAssociations(irrelevant_origin, signature, type);
    // Fill `expected_associations` depending on `type`.
    if (base::IsAsciiLower(test[i]))
      continue;
    if (type == FormAssociator::FormType::kAddressForm) {
      if (expected_associations.last_address_form_submitted) {
        // There should be at most two address form associations expected.
        EXPECT_FALSE(expected_associations.second_last_address_form_submitted);
        expected_associations.second_last_address_form_submitted =
            expected_associations.last_address_form_submitted;
      }
      expected_associations.last_address_form_submitted = signature;
    } else {
      // There should be at most one credit card form association expected.
      EXPECT_FALSE(expected_associations.last_credit_card_form_submitted);
      expected_associations.last_credit_card_form_submitted = signature;
    }
  }

  auto associations =
      form_associator.GetFormAssociations(FormSignature{test.size() - 1});
  EXPECT_TRUE(associations);
  EXPECT_EQ(expected_associations.last_address_form_submitted,
            associations->last_address_form_submitted);
  EXPECT_EQ(expected_associations.second_last_address_form_submitted,
            associations->second_last_address_form_submitted);
  EXPECT_EQ(expected_associations.last_credit_card_form_submitted,
            associations->last_credit_card_form_submitted);
}

}  // namespace
}  // namespace autofill
