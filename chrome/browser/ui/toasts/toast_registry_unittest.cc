// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/toasts/toast_specification.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/vector_icon_types.h"

class ToastRegistryTest : public testing::Test {};

// By default, the toast specification must have an icon and body string.
TEST_F(ToastRegistryTest, DefaultToast) {
  const int string_id = IDS_MEMORY_SAVER_DIALOG_TITLE;
  std::unique_ptr<ToastSpecification> spec =
      ToastSpecification::Builder(vector_icons::kEmailIcon, string_id).Build();

  EXPECT_EQ(string_id, spec->body_string_id());
  EXPECT_FALSE(spec->has_close_button());
  EXPECT_FALSE(spec->action_button_string_id().has_value());
  EXPECT_TRUE(spec->action_button_callback().is_null());
  EXPECT_EQ(spec->menu_model(), nullptr);
  EXPECT_FALSE(spec->is_persistent_toast());
}

TEST_F(ToastRegistryTest, ToastWithCloseButton) {
  const int string_id = IDS_MEMORY_SAVER_DIALOG_TITLE;
  std::unique_ptr<ToastSpecification> spec =
      ToastSpecification::Builder(vector_icons::kEmailIcon, string_id)
          .AddCloseButton()
          .Build();

  EXPECT_EQ(string_id, spec->body_string_id());
  EXPECT_TRUE(spec->has_close_button());
  EXPECT_FALSE(spec->action_button_string_id().has_value());
  EXPECT_TRUE(spec->action_button_callback().is_null());
  EXPECT_EQ(spec->menu_model(), nullptr);
  EXPECT_FALSE(spec->is_persistent_toast());
}

TEST_F(ToastRegistryTest, ToastWithActionButton) {
  const int body_string_id = IDS_MEMORY_SAVER_DIALOG_TITLE;
  const int action_button_string_id =
      IDS_PERFORMANCE_INTERVENTION_DEACTIVATE_TABS_BUTTON_V1;
  std::unique_ptr<ToastSpecification> spec =
      ToastSpecification::Builder(vector_icons::kEmailIcon, body_string_id)
          .AddActionButton(action_button_string_id, base::DoNothing())
          .AddCloseButton()
          .Build();
  EXPECT_EQ(body_string_id, spec->body_string_id());
  EXPECT_TRUE(spec->has_close_button());
  EXPECT_TRUE(spec->action_button_string_id().has_value());
  EXPECT_EQ(action_button_string_id, spec->action_button_string_id().value());
  EXPECT_FALSE(spec->action_button_callback().is_null());
  EXPECT_EQ(spec->menu_model(), nullptr);
  EXPECT_FALSE(spec->is_persistent_toast());

  // Toasts with an action button must have a close button.
  EXPECT_DEATH(
      ToastSpecification::Builder(vector_icons::kEmailIcon, body_string_id)
          .AddActionButton(action_button_string_id, base::DoNothing())
          .Build(),
      "");

  // A toast cannot have an action button, close button, and a menu.
  EXPECT_DEATH(
      ToastSpecification::Builder(vector_icons::kEmailIcon, body_string_id)
          .AddActionButton(action_button_string_id, base::DoNothing())
          .AddCloseButton()
          .AddMenu(std::make_unique<ui::SimpleMenuModel>(nullptr))
          .Build(),
      "");
}

TEST_F(ToastRegistryTest, ToastWithMenu) {
  const int body_string_id = IDS_MEMORY_SAVER_DIALOG_TITLE;
  std::unique_ptr<ToastSpecification> spec =
      ToastSpecification::Builder(vector_icons::kEmailIcon, body_string_id)
          .AddMenu(std::make_unique<ui::SimpleMenuModel>(nullptr))
          .Build();
  EXPECT_EQ(body_string_id, spec->body_string_id());
  EXPECT_FALSE(spec->has_close_button());
  EXPECT_FALSE(spec->action_button_string_id().has_value());
  EXPECT_TRUE(spec->action_button_callback().is_null());
  EXPECT_NE(spec->menu_model(), nullptr);
  EXPECT_FALSE(spec->is_persistent_toast());
}

TEST_F(ToastRegistryTest, PersistentToast) {
  const int body_string_id = IDS_MEMORY_SAVER_DIALOG_TITLE;
  std::unique_ptr<ToastSpecification> spec =
      ToastSpecification::Builder(vector_icons::kEmailIcon, body_string_id)
          .AddPersistance()
          .Build();
  EXPECT_EQ(body_string_id, spec->body_string_id());
  EXPECT_FALSE(spec->has_close_button());
  EXPECT_FALSE(spec->action_button_string_id().has_value());
  EXPECT_TRUE(spec->action_button_callback().is_null());
  EXPECT_EQ(spec->menu_model(), nullptr);
  EXPECT_TRUE(spec->is_persistent_toast());
}
