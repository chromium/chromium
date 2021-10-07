// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/actions/action_tap_key.h"
#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace arc {
namespace input_overlay {

class ActionTapKeyTest : public testing::Test {
 protected:
  ActionTapKeyTest() = default;
};

constexpr const char kValidJson[] =
    R"json({
      "name": "Fight",
      "key": "KeyA",
      "location": {
        "position": [
          {
            "anchor": [
              0,
              0
            ],
            "anchor_to_target": [
              0.5,
              0.5
            ]
          },
          {
            "anchor": [
              0,
              0
            ],
            "anchor_to_target": [
              0.8,
              0.8
            ]
          }
        ]
      }
    })json";

constexpr const char kInValidJsonWrongKey[] =
    R"json({
      "name": "Fight",
      "key": "Key_A",
      "location": {
        "position": [
          {
            "anchor": [
              0,
              0
            ],
            "anchor_to_target": [
              0.5,
              0.5
            ]
          }
        ]
      }
    })json";

constexpr const char kInValidJsonEmptyLocation[] =
    R"json({
      "name": "Fight",
      "key": "KeyA"
    })json";

TEST(ActionTapKeyTest, TestParseJson) {
  // Parse valid Json.
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  aura::test::TestWindowDelegate dummy_delegate;
  auto window = base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 11, gfx::Rect(200, 400), nullptr));
  std::unique_ptr<ActionTapKey> action =
      std::make_unique<ActionTapKey>(window.get());
  EXPECT_TRUE(action->ParseFromJson(json_value.value.value()));
  EXPECT_TRUE(action->key() == ui::DomCode::US_A);
  EXPECT_TRUE(action->name() == std::string("Fight"));
  EXPECT_TRUE(action->locations().size() == 2);
  action.reset();

  // Parse invalid Json with wrong key code.
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInValidJsonWrongKey);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  action = std::make_unique<ActionTapKey>(window.get());
  EXPECT_FALSE(action->ParseFromJson(json_value.value.value()));
  action.reset();

  // Parse invalid Json with no location.
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInValidJsonEmptyLocation);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  action = std::make_unique<ActionTapKey>(window.get());
  EXPECT_FALSE(action->ParseFromJson(json_value.value.value()));
  action.reset();
}

}  // namespace input_overlay
}  // namespace arc
