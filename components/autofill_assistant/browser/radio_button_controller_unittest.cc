// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/radio_button_controller.h"

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class RadioButtonControllerTest : public testing::Test {
 protected:
  RadioButtonControllerTest() : controller_(&user_model_) {}
  ~RadioButtonControllerTest() override {}

  base::flat_map<std::string, base::flat_set<std::string>> GetRadioGroups() {
    return controller_.radio_groups_;
  }

  void SetRadioGroups(
      const base::flat_map<std::string, base::flat_set<std::string>>&
          radio_groups) {
    controller_.radio_groups_ = radio_groups;
  }

  UserModel user_model_;
  RadioButtonController controller_;
};

TEST_F(RadioButtonControllerTest, AddRadioButtonToGroup) {
  EXPECT_THAT(GetRadioGroups(), SizeIs(0));
  controller_.AddRadioButtonToGroup("group_1", "id_1");
  EXPECT_THAT(GetRadioGroups(),
              UnorderedElementsAre(
                  Pair("group_1", base::flat_set<std::string>{"id_1"})));

  controller_.AddRadioButtonToGroup("group_1", "id_1");
  EXPECT_THAT(GetRadioGroups(),
              UnorderedElementsAre(
                  Pair("group_1", base::flat_set<std::string>{"id_1"})));

  controller_.AddRadioButtonToGroup("group_1", "id_2");
  EXPECT_THAT(GetRadioGroups(),
              UnorderedElementsAre(Pair(
                  "group_1", base::flat_set<std::string>{"id_1", "id_2"})));

  controller_.AddRadioButtonToGroup("group_2", "id_3");
  EXPECT_THAT(GetRadioGroups(),
              UnorderedElementsAre(
                  Pair("group_1", base::flat_set<std::string>{"id_1", "id_2"}),
                  Pair("group_2", base::flat_set<std::string>{"id_3"})));
}

TEST_F(RadioButtonControllerTest, RemoveRadioButtonFromGroup) {
  SetRadioGroups(
      {{"group_1", {"id_1", "id_2"}}, {"group_2", {"id_3", "id_4"}}});

  // Does not create group_3 or remove id_1 from a different group.
  controller_.RemoveRadioButtonFromGroup("group_3", "id_1");
  EXPECT_THAT(
      GetRadioGroups(),
      UnorderedElementsAre(
          Pair("group_1", base::flat_set<std::string>{"id_1", "id_2"}),
          Pair("group_2", base::flat_set<std::string>{"id_3", "id_4"})));

  controller_.RemoveRadioButtonFromGroup("group_1", "id_2");
  controller_.RemoveRadioButtonFromGroup("group_2", "id_3");
  EXPECT_THAT(GetRadioGroups(),
              UnorderedElementsAre(
                  Pair("group_1", base::flat_set<std::string>{"id_1"}),
                  Pair("group_2", base::flat_set<std::string>{"id_4"})));
}

TEST_F(RadioButtonControllerTest, UpdateRadioButtonGroup) {
  controller_.AddRadioButtonToGroup("group_1", "id_1");
  controller_.AddRadioButtonToGroup("group_1", "id_2");
  controller_.AddRadioButtonToGroup("group_1", "id_3");
  controller_.AddRadioButtonToGroup("group_2", "id_4");
  controller_.AddRadioButtonToGroup("group_2", "id_5");

  EXPECT_FALSE(controller_.UpdateRadioButtonGroup("does_not_exist", "id_1"));

  EXPECT_TRUE(controller_.UpdateRadioButtonGroup("group_1", "id_1"));
  EXPECT_EQ(user_model_.GetValue("id_1"), SimpleValue(true));
  EXPECT_EQ(user_model_.GetValue("id_2"), SimpleValue(false));
  EXPECT_EQ(user_model_.GetValue("id_3"), SimpleValue(false));

  EXPECT_FALSE(controller_.UpdateRadioButtonGroup("group_1", "does_not_exist"));
  EXPECT_EQ(user_model_.GetValue("id_1"), SimpleValue(true));
  EXPECT_EQ(user_model_.GetValue("id_2"), SimpleValue(false));
  EXPECT_EQ(user_model_.GetValue("id_3"), SimpleValue(false));

  EXPECT_TRUE(controller_.UpdateRadioButtonGroup("group_2", "id_5"));
  EXPECT_EQ(user_model_.GetValue("id_1"), SimpleValue(true));
  EXPECT_EQ(user_model_.GetValue("id_2"), SimpleValue(false));
  EXPECT_EQ(user_model_.GetValue("id_3"), SimpleValue(false));
  EXPECT_EQ(user_model_.GetValue("id_4"), SimpleValue(false));
  EXPECT_EQ(user_model_.GetValue("id_5"), SimpleValue(true));
}

}  // namespace autofill_assistant
