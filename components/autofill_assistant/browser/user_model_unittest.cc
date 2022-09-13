// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_model.h"

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/mock_user_model_observer.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
const char kFakeUrl[] = "https://www.example.com";

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Pair;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class UserModelTest : public testing::Test {
 public:
  UserModelTest() = default;
  ~UserModelTest() override {}

  void SetUp() override { model_.AddObserver(&mock_observer_); }

  void TearDown() override { model_.RemoveObserver(&mock_observer_); }

  // Provides direct access to the values in the model for testing.
  const base::flat_map<std::string, ValueProto>& GetValues() const {
    return model_.values_;
  }

  ValueProto CreateStringValue() const {
    ValueProto value;
    value.mutable_strings()->add_values("Aurea prima");
    value.mutable_strings()->add_values("sata est,");
    value.mutable_strings()->add_values("aetas quae");
    value.mutable_strings()->add_values("vindice nullo");
    value.mutable_strings()->add_values("ü万𠜎");
    return value;
  }

  ValueProto CreateIntValue() const {
    ValueProto value;
    value.mutable_ints()->add_values(1);
    value.mutable_ints()->add_values(123);
    value.mutable_ints()->add_values(5);
    value.mutable_ints()->add_values(-132);
    return value;
  }

  ValueProto CreateBoolValue() const {
    ValueProto value;
    value.mutable_booleans()->add_values(true);
    value.mutable_booleans()->add_values(false);
    value.mutable_booleans()->add_values(true);
    value.mutable_booleans()->add_values(true);
    return value;
  }

 protected:
  UserModel model_;
  MockUserModelObserver mock_observer_;
};

TEST_F(UserModelTest, EmptyValue) {
  ValueProto value;
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", value)).Times(1);
  model_.SetValue("identifier", value);
  model_.SetValue("identifier", value);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(Pair("identifier", value)));
}

TEST_F(UserModelTest, InsertNewValues) {
  ValueProto value_a = CreateStringValue();
  ValueProto value_b = CreateIntValue();
  ValueProto value_c = CreateBoolValue();

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnValueChanged("value_a", value_a)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("value_b", value_b)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("value_c", value_c)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged(_, _)).Times(0);

  model_.SetValue("value_a", value_a);
  model_.SetValue("value_b", value_b);
  model_.SetValue("value_c", value_c);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(Pair("value_a", value_a),
                                                Pair("value_b", value_b),
                                                Pair("value_c", value_c)));
}

TEST_F(UserModelTest, OverwriteWithExistingValueFiresNoChangeEvent) {
  ValueProto value = CreateStringValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", value)).Times(1);
  model_.SetValue("identifier", value);

  ValueProto same_value = CreateStringValue();
  model_.SetValue("identifier", same_value);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(Pair("identifier", value)));
}

TEST_F(UserModelTest, OverwriteWithDifferentValueFiresChangeEvent) {
  ValueProto value = CreateStringValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", _)).Times(2);
  model_.SetValue("identifier", value);

  ValueProto another_value = CreateStringValue();
  another_value.mutable_strings()->add_values("tomato");
  model_.SetValue("identifier", another_value);

  EXPECT_THAT(GetValues(),
              UnorderedElementsAre(Pair("identifier", another_value)));
}

TEST_F(UserModelTest, ForceNotificationAlwaysFiresChangeEvent) {
  testing::InSequence seq;
  ValueProto value_a = CreateStringValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(1);
  model_.SetValue("a", value_a);

  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(0);
  model_.SetValue("a", value_a);

  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(1);
  model_.SetValue("a", value_a, /* force_notification = */ true);
}

TEST_F(UserModelTest, MergeWithProto) {
  testing::InSequence seq;
  ValueProto value_a = CreateStringValue();
  ValueProto value_b = CreateIntValue();
  ValueProto value_d = CreateBoolValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("b", value_b)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("c", ValueProto())).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("d", value_d)).Times(1);
  model_.SetValue("a", value_a);
  model_.SetValue("b", value_b);
  model_.SetValue("c", ValueProto());
  model_.SetValue("d", value_d);

  ModelProto proto;
  ValueProto value_b_changed = value_b;
  value_b_changed.mutable_ints()->add_values(14);
  ValueProto value_c_changed = CreateBoolValue();
  ValueProto value_e = CreateStringValue();
  // Overwrite existing value.
  auto* value = proto.add_values();
  value->set_identifier("b");
  *value->mutable_value() = value_b_changed;
  // Overwrite existing empty value with non-empty value.
  value = proto.add_values();
  value->set_identifier("c");
  *value->mutable_value() = value_c_changed;
  // Does not overwrite existing non-empty value.
  value = proto.add_values();
  value->set_identifier("d");
  *value->mutable_value() = ValueProto();
  // Inserts new non-empty value.
  value = proto.add_values();
  value->set_identifier("e");
  *value->mutable_value() = value_e;
  // Inserts new empty value.
  value = proto.add_values();
  value->set_identifier("f");
  *value->mutable_value() = ValueProto();

  EXPECT_CALL(mock_observer_, OnValueChanged("a", _)).Times(0);
  EXPECT_CALL(mock_observer_, OnValueChanged("b", value_b_changed)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("c", value_c_changed)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("d", _)).Times(0);
  EXPECT_CALL(mock_observer_, OnValueChanged("e", value_e)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("f", ValueProto())).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged(_, _)).Times(0);
  model_.MergeWithProto(proto, /*force_notification=*/false);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(
                               Pair("a", value_a), Pair("b", value_b_changed),
                               Pair("c", value_c_changed), Pair("d", value_d),
                               Pair("e", value_e), Pair("f", ValueProto())));
}

TEST_F(UserModelTest, UpdateProto) {
  testing::InSequence seq;
  ValueProto value_a = CreateStringValue();
  ValueProto value_c = CreateBoolValue();
  model_.SetValue("a", value_a);
  model_.SetValue("b", ValueProto());
  model_.SetValue("c", value_c);
  model_.SetValue("d", CreateStringValue());

  ModelProto proto;
  // 'a' in proto is non-empty value and should be overwritten.
  auto* value = proto.add_values();
  value->set_identifier("a");
  *value->mutable_value() = CreateBoolValue();
  // 'b' in proto is non-empty and should be overwritten with empty value.
  value = proto.add_values();
  value->set_identifier("b");
  *value->mutable_value() = CreateIntValue();
  // 'c' in proto is default empty and should be overwritten with |value_c|.
  proto.add_values()->set_identifier("c");
  // 'd' is not in the proto and should not be added to the proto.

  model_.UpdateProto(&proto);
  EXPECT_THAT(
      proto.values(),
      UnorderedElementsAre(
          AllOf(Property(&ModelProto::ModelValue::identifier, StrEq("a")),
                Property(&ModelProto::ModelValue::value, Eq(value_a))),
          AllOf(Property(&ModelProto::ModelValue::identifier, StrEq("b")),
                Property(&ModelProto::ModelValue::value, Eq(ValueProto()))),
          AllOf(Property(&ModelProto::ModelValue::identifier, StrEq("c")),
                Property(&ModelProto::ModelValue::value, Eq(value_c)))));
}

TEST_F(UserModelTest, SubscriptAccess) {
  ValueProto value;
  value.mutable_strings()->add_values("a");
  value.mutable_strings()->add_values("b");
  value.mutable_strings()->add_values("c");

  model_.SetValue("value", value);
  EXPECT_EQ(model_.GetValue("value"), value);
  EXPECT_EQ(model_.GetValue("value[0]"), SimpleValue(std::string("a")));
  EXPECT_EQ(model_.GetValue("value[1]"), SimpleValue(std::string("b")));
  EXPECT_EQ(model_.GetValue("value[2]"), SimpleValue(std::string("c")));
  EXPECT_EQ(model_.GetValue("value[001]"), SimpleValue(std::string("b")));

  EXPECT_EQ(model_.GetValue("value[3]"), absl::nullopt);
  EXPECT_EQ(model_.GetValue("value[-1]"), absl::nullopt);

  model_.SetValue("index", SimpleValue(0));
  EXPECT_EQ(model_.GetValue("value[index]"), SimpleValue(std::string("a")));
  model_.SetValue("index", SimpleValue(1));
  EXPECT_EQ(model_.GetValue("value[index]"), SimpleValue(std::string("b")));
  model_.SetValue("index", SimpleValue(2));
  EXPECT_EQ(model_.GetValue("value[index]"), SimpleValue(std::string("c")));
  model_.SetValue("index", SimpleValue(3));
  EXPECT_EQ(model_.GetValue("value[index]"), absl::nullopt);
  model_.SetValue("index", SimpleValue(-1));
  EXPECT_EQ(model_.GetValue("value[index]"), absl::nullopt);

  model_.SetValue("index", SimpleValue(0));
  EXPECT_EQ(model_.GetValue("value[index[0]]"), SimpleValue(std::string("a")));
  model_.SetValue("index", SimpleValue(std::string("not an index")));
  EXPECT_EQ(model_.GetValue("value[index]"), absl::nullopt);

  ValueProto indices;
  indices.mutable_ints()->add_values(2);
  indices.mutable_ints()->add_values(0);
  indices.mutable_ints()->add_values(1);
  model_.SetValue("indices", indices);

  model_.SetValue("index", SimpleValue(0));
  EXPECT_EQ(model_.GetValue("value[indices[index]]"),
            SimpleValue(std::string("c")));

  model_.SetValue("index", SimpleValue(1));
  EXPECT_EQ(model_.GetValue("value[indices[index]]"),
            SimpleValue(std::string("a")));

  model_.SetValue("index", SimpleValue(2));
  EXPECT_EQ(model_.GetValue("value[indices[index]]"),
            SimpleValue(std::string("b")));
}

TEST_F(UserModelTest, IrregularModelIdentifiers) {
  ValueProto value;
  value.mutable_strings()->add_values("a");
  value.mutable_strings()->add_values("b");
  value.mutable_strings()->add_values("c");

  model_.SetValue("normal_identifier", value);
  model_.SetValue("utf_8_ü万𠜎", value);
  model_.SetValue("ends_in_bracket]", value);
  model_.SetValue("contains_[brackets]", value);
  model_.SetValue("[]", value);
  model_.SetValue("empty_brackets[]", value);

  // Retrieving simple values works for any identifiers.
  EXPECT_EQ(model_.GetValue("normal_identifier"), value);
  EXPECT_EQ(model_.GetValue("utf_8_ü万𠜎"), value);
  EXPECT_EQ(model_.GetValue("ends_in_bracket]"), value);
  EXPECT_EQ(model_.GetValue("contains_[brackets]"), value);
  EXPECT_EQ(model_.GetValue("[]"), value);
  EXPECT_EQ(model_.GetValue("empty_brackets[]"), value);

  // Subscript access is not supported for model identifiers containing
  // irregular characters (i.e., outside of \w+).
  EXPECT_EQ(model_.GetValue("normal_identifier[1]"),
            SimpleValue(std::string("b")));
  EXPECT_EQ(model_.GetValue("ends_in_bracket][1]"), absl::nullopt);
  EXPECT_EQ(model_.GetValue("contains_[brackets][1]"), absl::nullopt);
  EXPECT_EQ(model_.GetValue("[][0]"), absl::nullopt);
  EXPECT_EQ(model_.GetValue("empty_brackets[1]"), absl::nullopt);
  EXPECT_EQ(model_.GetValue("empty_brackets[][1]"), absl::nullopt);

  // Subscript access into UTF-8 identifiers is not supported.
  EXPECT_EQ(model_.GetValue("utf_8_ü万𠜎[1]"), absl::nullopt);
}

TEST_F(UserModelTest, SetCreditCards) {
  autofill::CreditCard credit_card_a(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card_a, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  AutofillCreditCardProto credit_card_a_proto;
  credit_card_a_proto.set_guid(credit_card_a.guid());
  autofill::CreditCard credit_card_b(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card_b, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");
  AutofillCreditCardProto credit_card_b_proto;
  credit_card_b_proto.set_guid(credit_card_b.guid());

  auto credit_cards =
      std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card_a));
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card_b));
  model_.SetAutofillCreditCards(std::move(credit_cards));
  EXPECT_THAT(model_.GetCreditCard(credit_card_a_proto)->Compare(credit_card_a),
              Eq(0));
  EXPECT_THAT(model_.GetCreditCard(credit_card_b_proto)->Compare(credit_card_b),
              Eq(0));
}

TEST_F(UserModelTest, SetProfiles) {
  autofill::AutofillProfile profile_a(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile_a, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");
  autofill::AutofillProfile profile_b(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&profile_b, "John", "", "Doe",
                                 "editor@gmail.com", "", "203 Barfield Lane",
                                 "", "Mountain View", "CA", "94043", "US",
                                 "+12345678901");
  auto profiles = std::make_unique<
      std::vector<std::unique_ptr<autofill::AutofillProfile>>>();
  profiles->emplace_back(
      std::make_unique<autofill::AutofillProfile>(profile_a));
  profiles->emplace_back(
      std::make_unique<autofill::AutofillProfile>(profile_b));
  model_.SetAutofillProfiles(std::move(profiles));
  AutofillProfileProto profile_a_proto;
  profile_a_proto.set_guid(profile_a.guid());
  AutofillProfileProto profile_b_proto;
  profile_b_proto.set_guid(profile_b.guid());

  EXPECT_THAT(model_.GetProfile(profile_a_proto)->Compare(profile_a), Eq(0));
  EXPECT_THAT(model_.GetProfile(profile_b_proto)->Compare(profile_b), Eq(0));
}

TEST_F(UserModelTest, ClientSideOnlyNotifications) {
  testing::InSequence seq;
  model_.SetValue("identifier",
                  SimpleValue(1, /* is_client_side_only = */ false));
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", _)).Times(0);
  model_.SetValue("identifier",
                  SimpleValue(1, /* is_client_side_only = */ false));

  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", _)).Times(1);
  model_.SetValue("identifier",
                  SimpleValue(1, /* is_client_side_only = */ true));

  EXPECT_TRUE(GetValues().at("identifier").is_client_side_only());
}

TEST_F(UserModelTest, SetSelectedAutofillProfile) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");

  UserData user_data;
  model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  EXPECT_THAT(model_.GetSelectedAutofillProfile("contact")->Compare(profile),
              Eq(0));
  EXPECT_THAT(user_data.selected_address("contact")->Compare(profile), Eq(0));
  model_.SetSelectedAutofillProfile("contact", nullptr, &user_data);
  EXPECT_THAT(model_.GetSelectedAutofillProfile("contact"), IsNull());
  EXPECT_THAT(user_data.selected_address("contact"), IsNull());
}

TEST_F(UserModelTest, GetProfileByProfileName) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");

  UserData user_data;
  model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(profile),
      &user_data);
  AutofillProfileProto profile_proto;
  profile_proto.set_selected_profile_name("contact");
  EXPECT_THAT(model_.GetProfile(profile_proto)->Compare(profile), Eq(0));
}

TEST_F(UserModelTest, GetSelectedCardWithProto) {
  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  UserData user_data;
  model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(credit_card), &user_data);
  AutofillCreditCardProto credit_card_proto;
  EXPECT_THAT(model_.GetCreditCard(credit_card_proto), IsNull());
  credit_card_proto.mutable_selected_credit_card();
  EXPECT_THAT(model_.GetCreditCard(credit_card_proto)->Compare(credit_card),
              Eq(0));
}

TEST_F(UserModelTest, SetSelectedCreditCard) {
  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  UserData user_data;
  model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(credit_card), &user_data);
  EXPECT_THAT(model_.GetSelectedCreditCard()->Compare(credit_card), Eq(0));
  EXPECT_THAT(user_data.selected_card()->Compare(credit_card), Eq(0));
  model_.SetSelectedCreditCard(nullptr, &user_data);
  EXPECT_THAT(model_.GetSelectedCreditCard(), IsNull());
  EXPECT_THAT(user_data.selected_card(), IsNull());
}

TEST_F(UserModelTest, SetSelectedLoginChoiceObject) {
  LoginChoice login_choice;
  login_choice.identifier = "guest";

  UserData user_data;
  model_.SetSelectedLoginChoice(std::make_unique<LoginChoice>(login_choice),
                                &user_data);
  EXPECT_THAT(user_data.selected_login_choice()->identifier, "guest");

  model_.SetSelectedLoginChoice(nullptr, &user_data);
  EXPECT_THAT(user_data.selected_login_choice(), IsNull());
}

TEST_F(UserModelTest, SetSelectedLoginChoiceIdentifier) {
  LoginChoice login_choice;
  login_choice.identifier = "guest";
  CollectUserDataOptions collect_user_data_options;
  collect_user_data_options.login_choices.push_back(login_choice);

  UserData user_data;
  model_.SetSelectedLoginChoiceByIdentifier("guest", collect_user_data_options,
                                            &user_data);
  EXPECT_THAT(user_data.selected_login_choice()->identifier, "guest");

  model_.SetSelectedLoginChoiceByIdentifier(
      "not found", collect_user_data_options, &user_data);
  EXPECT_THAT(user_data.selected_login_choice(), IsNull());
}

}  // namespace autofill_assistant
