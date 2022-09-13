// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace value_util {

namespace {
ValueProto CreateStringValue() {
  ValueProto value;
  value.mutable_strings()->add_values("Aurea prima");
  value.mutable_strings()->add_values("sata est,");
  value.mutable_strings()->add_values("aetas quae");
  value.mutable_strings()->add_values("vindice nullo");
  value.mutable_strings()->add_values("ü万𠜎");
  return value;
}

ValueProto CreateIntValue() {
  ValueProto value;
  value.mutable_ints()->add_values(1);
  value.mutable_ints()->add_values(123);
  value.mutable_ints()->add_values(5);
  value.mutable_ints()->add_values(-132);
  return value;
}

ValueProto CreateBoolValue() {
  ValueProto value;
  value.mutable_booleans()->add_values(true);
  value.mutable_booleans()->add_values(false);
  value.mutable_booleans()->add_values(true);
  value.mutable_booleans()->add_values(true);
  return value;
}

DateProto CreateDateProto(int year, int month, int day) {
  DateProto proto;
  proto.set_year(year);
  proto.set_month(month);
  proto.set_day(day);
  return proto;
}
}  // namespace

class ValueUtilTest : public testing::Test {
 public:
  ValueUtilTest() = default;
  ~ValueUtilTest() override {}
};

TEST_F(ValueUtilTest, DifferentTypesComparison) {
  ValueProto value_a;
  ValueProto value_b = CreateStringValue();
  ValueProto value_c = CreateIntValue();
  ValueProto value_d = CreateBoolValue();
  ValueProto value_e = SimpleValue(CreateDateProto(2020, 8, 30));
  ValueProto value_f;
  value_f.set_server_payload("payload");

  std::vector<ValueProto> values = {value_a, value_b, value_c,
                                    value_d, value_e, value_f};
  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = 0; j < values.size(); ++j) {
      if (j == i) {
        EXPECT_TRUE(values[i] == values[j]);
        continue;
      }
      EXPECT_FALSE(values[i] == values[j]);
    }
  }
}

TEST_F(ValueUtilTest, EmptyValueComparison) {
  ValueProto value_a;
  ValueProto value_b;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_strings()->add_values("potato");
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_strings()->clear_values();
  EXPECT_FALSE(value_a == value_b);

  value_a.clear_kind();
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, StringComparison) {
  ValueProto value_a = CreateStringValue();
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_strings()->add_values("potato");
  EXPECT_FALSE(value_a == value_b);

  value_b.mutable_strings()->add_values("tomato");
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_strings()->set_values(value_a.strings().values_size() - 1,
                                        "tomato");
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, IntComparison) {
  ValueProto value_a = CreateIntValue();
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_ints()->add_values(1);
  value_b.mutable_ints()->add_values(0);
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_ints()->set_values(value_a.ints().values_size() - 1, 0);
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, BoolComparison) {
  ValueProto value_a = CreateBoolValue();
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_booleans()->add_values(true);
  value_b.mutable_booleans()->add_values(false);
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_booleans()->set_values(value_a.booleans().values_size() - 1,
                                         false);
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, DateComparison) {
  ValueProto value_a = SimpleValue(CreateDateProto(2020, 4, 18));
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  *value_a.mutable_dates()->add_values() = CreateDateProto(2020, 6, 14);
  *value_b.mutable_dates()->add_values() = CreateDateProto(2020, 6, 15);
  EXPECT_FALSE(value_a == value_b);

  *value_b.mutable_dates()->mutable_values(1) = CreateDateProto(2020, 6, 14);
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, UserActionComparison) {
  ValueProto user_actions_value;
  UserActionProto user_action_a;
  user_action_a.set_identifier("identifier");
  user_action_a.mutable_chip()->set_type(ChipType::HIGHLIGHTED_ACTION);
  user_action_a.mutable_chip()->set_text("text");
  UserActionProto user_action_b = user_action_a;

  ValueProto value_a;
  *value_a.mutable_user_actions()->add_values() = user_action_a;
  ValueProto value_b;
  *value_b.mutable_user_actions()->add_values() = user_action_b;
  EXPECT_TRUE(value_a == value_b);

  value_b.mutable_user_actions()->mutable_values(0)->set_enabled(false);
  EXPECT_FALSE(value_a == value_b);

  value_b = value_a;
  value_b.mutable_user_actions()->mutable_values(0)->set_identifier("test");
  EXPECT_FALSE(value_a == value_b);
}

TEST_F(ValueUtilTest, CreditCardComparison) {
  ValueProto value_a;
  value_a.mutable_credit_cards()->add_values()->set_guid("GUID");
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_b.mutable_credit_cards()->mutable_values(0)->set_guid("wrong");
  EXPECT_FALSE(value_a == value_b);
}

TEST_F(ValueUtilTest, ProfileComparison) {
  ValueProto value_a;
  value_a.mutable_profiles()->add_values()->set_guid("GUID");
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_b.mutable_profiles()->mutable_values(0)->set_guid("wrong");
  EXPECT_FALSE(value_a == value_b);
}

TEST_F(ValueUtilTest, LoginOptionComparison) {
  ValueProto value_a;
  auto* option_a = value_a.mutable_login_options()->add_values();
  option_a->set_label("label");
  option_a->set_sublabel("sublabel");
  option_a->set_payload("payload");
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  option_a->set_label("different");
  EXPECT_FALSE(value_a == value_b);

  option_a->set_label("label");
  option_a->set_sublabel("different");
  EXPECT_FALSE(value_a == value_b);

  option_a->set_sublabel("sublabel");
  option_a->set_payload("different");
  EXPECT_FALSE(value_a == value_b);
}

TEST_F(ValueUtilTest, CreditCardResponseComparison) {
  ValueProto value_a;
  value_a.mutable_credit_card_response()->set_network("network");
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_b.mutable_credit_card_response()->set_network("different");
  EXPECT_FALSE(value_a == value_b);
}

TEST_F(ValueUtilTest, ServerPayloadComparison) {
  ValueProto value_a;
  value_a.set_server_payload("payload");
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_b.set_server_payload("different");
  EXPECT_FALSE(value_a == value_b);
}

TEST_F(ValueUtilTest, AreAllValuesOfType) {
  ValueProto value_a;
  ValueProto value_b;
  ValueProto value_c;
  EXPECT_TRUE(AreAllValuesOfType({value_a, value_b, value_c},
                                 ValueProto::KIND_NOT_SET));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kStrings));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kBooleans));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kInts));

  value_a = SimpleValue(std::string(""));
  value_b = SimpleValue(std::string("non-empty"));
  EXPECT_TRUE(AreAllValuesOfType({value_a, value_b}, ValueProto::kStrings));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kStrings));

  value_c = CreateStringValue();
  EXPECT_TRUE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kStrings));
}

TEST_F(ValueUtilTest, AreAllValuesOfSize) {
  // Not-set values have size 0.
  ValueProto value_a;
  ValueProto value_b;
  ValueProto value_c;
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b, value_c}, 0));

  value_a = SimpleValue(std::string(""));
  value_b = SimpleValue(std::string("non-empty"));
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b}, 1));

  value_c = SimpleValue(std::string("another"));
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b, value_c}, 1));

  value_c.mutable_strings()->add_values(std::string("second value"));
  EXPECT_FALSE(AreAllValuesOfSize({value_a, value_b, value_c}, 1));

  value_a.mutable_strings()->add_values(std::string(""));
  value_b.mutable_strings()->add_values(std::string("test"));
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b, value_c}, 2));
}

TEST_F(ValueUtilTest, SmallerOperatorForValueProto) {
  EXPECT_TRUE(SimpleValue(1) < SimpleValue(2));
  EXPECT_TRUE(SimpleValue(std::string("a")) < SimpleValue(std::string("b")));
  EXPECT_TRUE(SimpleValue(CreateDateProto(2020, 4, 19)) <
              SimpleValue(CreateDateProto(2020, 4, 20)));
  EXPECT_TRUE(SimpleValue(CreateDateProto(2020, 3, 21)) <
              SimpleValue(CreateDateProto(2020, 4, 20)));
  EXPECT_TRUE(SimpleValue(CreateDateProto(2019, 5, 21)) <
              SimpleValue(CreateDateProto(2020, 4, 20)));

  EXPECT_FALSE(SimpleValue(2) < SimpleValue(1));
  EXPECT_FALSE(SimpleValue(std::string("b")) < SimpleValue(std::string("a")));
  EXPECT_FALSE(SimpleValue(CreateDateProto(2020, 4, 20)) <
               SimpleValue(CreateDateProto(2020, 4, 19)));
  EXPECT_FALSE(SimpleValue(CreateDateProto(2020, 4, 20)) <
               SimpleValue(CreateDateProto(2020, 3, 21)));
  EXPECT_FALSE(SimpleValue(CreateDateProto(2020, 4, 20)) <
               SimpleValue(CreateDateProto(2019, 5, 21)));

  EXPECT_FALSE(SimpleValue(1) < SimpleValue(1));
  EXPECT_FALSE(SimpleValue(std::string("a")) < SimpleValue(std::string("a")));
  EXPECT_FALSE(SimpleValue(CreateDateProto(2020, 4, 19)) <
               SimpleValue(CreateDateProto(2020, 4, 19)));

  // Empty values.
  ValueProto value_a;
  ValueProto value_b;
  EXPECT_FALSE(value_a < value_b || value_b < value_a);

  // Different types.
  value_a = SimpleValue(std::string("a"));
  value_b = SimpleValue(1);
  EXPECT_FALSE(value_a < value_b || value_b < value_a);

  // Size != 1.
  value_a = SimpleValue(1);
  value_b.mutable_booleans()->add_values(2);
  value_b.mutable_booleans()->add_values(3);
  EXPECT_FALSE(value_a < value_b || value_b < value_a);

  // Unsupported types.
  value_a.mutable_user_actions();
  value_b.mutable_user_actions();
  EXPECT_FALSE(value_a < value_b || value_b < value_a);

  value_a.mutable_booleans();
  value_b.mutable_booleans();
  EXPECT_FALSE(value_a < value_b || value_b < value_a);
}

TEST_F(ValueUtilTest, NotEqualOperatorForValueProto) {
  ValueProto value_a;
  ValueProto value_b;
  EXPECT_FALSE(value_a != value_b);

  value_a.mutable_strings()->add_values("potato");
  EXPECT_TRUE(value_a != value_b);

  value_a.mutable_strings()->clear_values();
  EXPECT_TRUE(value_a != value_b);

  value_a.clear_kind();
  EXPECT_FALSE(value_a != value_b);

  value_a = CreateStringValue();
  value_b = value_a;
  EXPECT_FALSE(value_a != value_b);

  value_a = CreateIntValue();
  value_b = value_a;
  EXPECT_FALSE(value_a != value_b);

  value_a.mutable_ints()->add_values(1);
  value_b.mutable_ints()->add_values(0);
  EXPECT_TRUE(value_a != value_b);

  value_a = CreateBoolValue();
  value_b = value_a;
  EXPECT_FALSE(value_a != value_b);

  value_a.mutable_booleans()->add_values(true);
  value_b.mutable_booleans()->add_values(false);
  EXPECT_TRUE(value_a != value_b);

  value_a = SimpleValue(CreateDateProto(2020, 4, 18));
  value_b = value_a;
  EXPECT_FALSE(value_a != value_b);

  *value_a.mutable_dates()->add_values() = CreateDateProto(2020, 6, 14);
  *value_b.mutable_dates()->add_values() = CreateDateProto(2020, 6, 15);
  EXPECT_TRUE(value_a != value_b);
}

TEST_F(ValueUtilTest, TestGetNthValue) {
  ValueProto value;
  value.mutable_strings()->add_values("a");
  value.mutable_strings()->add_values("b");
  value.mutable_strings()->add_values("c");

  EXPECT_EQ(GetNthValue(value, 0), SimpleValue(std::string("a")));
  EXPECT_EQ(GetNthValue(value, 1), SimpleValue(std::string("b")));
  EXPECT_EQ(GetNthValue(value, 2), SimpleValue(std::string("c")));

  EXPECT_EQ(GetNthValue(value, -1), absl::nullopt);
  EXPECT_EQ(GetNthValue(value, 3), absl::nullopt);

  value.set_is_client_side_only(true);
  EXPECT_EQ(GetNthValue(value, 0),
            SimpleValue(std::string("a"), /* is_client_side_only = */ true));
}

TEST_F(ValueUtilTest, TestContainsClientOnlyValues) {
  ValueProto value_a =
      SimpleValue(std::string("test"), /* is_client_side_only = */ false);
  ValueProto value_b = value_a;
  ValueProto value_c = value_a;
  EXPECT_FALSE(ContainsClientOnlyValue({}));
  EXPECT_FALSE(ContainsClientOnlyValue({value_a, value_b, value_c}));

  value_b.set_is_client_side_only(true);
  EXPECT_TRUE(ContainsClientOnlyValue({value_a, value_b, value_c}));
  EXPECT_FALSE(ContainsClientOnlyValue({value_a, value_c}));
}

TEST_F(ValueUtilTest, TestEqualOperatorForAutofillProfile) {
  AutofillProfileProto profile_a;
  AutofillProfileProto profile_b;
  EXPECT_TRUE(profile_a == profile_b);

  profile_a.set_guid("guid_a");
  EXPECT_FALSE(profile_a == profile_b);

  profile_b.set_guid("guid_b");
  EXPECT_FALSE(profile_a == profile_b);

  profile_b.set_guid("guid_a");
  EXPECT_TRUE(profile_a == profile_b);

  profile_a.clear_guid();
  profile_a.set_selected_profile_name("name_a");
  EXPECT_FALSE(profile_a == profile_b);

  profile_b.clear_guid();
  profile_b.set_selected_profile_name("name_b");
  EXPECT_FALSE(profile_a == profile_b);

  profile_b.set_selected_profile_name("name_a");
  EXPECT_TRUE(profile_a == profile_b);

  profile_a.clear_selected_profile_name();
  EXPECT_FALSE(profile_a == profile_b);
}

TEST_F(ValueUtilTest, TestEqualOperatorForAutofillCreditCard) {
  AutofillCreditCardProto credit_card_a;
  AutofillCreditCardProto credit_card_b;
  EXPECT_TRUE(credit_card_a == credit_card_b);

  credit_card_a.set_guid("guid_a");
  EXPECT_FALSE(credit_card_a == credit_card_b);

  credit_card_b.set_guid("guid_b");
  EXPECT_FALSE(credit_card_a == credit_card_b);

  credit_card_b.set_guid("guid_a");
  EXPECT_TRUE(credit_card_a == credit_card_b);

  credit_card_a.clear_guid();
  credit_card_a.mutable_selected_credit_card();
  EXPECT_FALSE(credit_card_a == credit_card_b);

  credit_card_b.clear_guid();
  credit_card_b.mutable_selected_credit_card();
  EXPECT_TRUE(credit_card_a == credit_card_b);

  credit_card_a.clear_selected_credit_card();
  EXPECT_FALSE(credit_card_a == credit_card_b);
}

}  // namespace value_util
}  // namespace autofill_assistant
