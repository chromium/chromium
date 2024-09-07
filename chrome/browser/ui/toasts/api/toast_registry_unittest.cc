// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/api/toast_registry.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/vector_icon_types.h"

class ToastRegistryTest : public testing::Test {};

// By default, the toast specification must have an icon and body string.
TEST_F(ToastRegistryTest, DefaultToast) {
  const int string_id = 0;
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
  const int string_id = 0;
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
  const int body_string_id = 0;
  const int action_button_string_id = 1;
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
  const int body_string_id = 0;
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
  const int body_string_id = 0;
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

TEST_F(ToastRegistryTest, RegisterSpecification) {
  std::unique_ptr<ToastSpecification> unique_spec =
      ToastSpecification::Builder(vector_icons::kEmailIcon,
                                  /*body_string_id=*/0)
          .AddPersistance()
          .Build();

  ToastSpecification* toast_specification = unique_spec.get();

  std::unique_ptr<ToastRegistry> toast_registry =
      std::make_unique<ToastRegistry>();

  toast_registry->RegisterToast(ToastId::kImageCopied, std::move(unique_spec));
  EXPECT_EQ(toast_specification,
            toast_registry->GetToastSpecification(ToastId::kImageCopied));
  EXPECT_FALSE(toast_registry->IsEmpty());
}

TEST_F(ToastRegistryTest, RegisterNullSpecification) {
  std::unique_ptr<ToastRegistry> toast_registry =
      std::make_unique<ToastRegistry>();

  // ToastRegistry should hit a CHECK when we try to register a null
  // ToastSpecification.
  EXPECT_DEATH(toast_registry->RegisterToast(ToastId::kImageCopied, nullptr),
               "");
}

TEST_F(ToastRegistryTest, RegisterDuplicateToastId) {
  std::unique_ptr<ToastRegistry> toast_registry =
      std::make_unique<ToastRegistry>();

  toast_registry->RegisterToast(
      ToastId::kImageCopied, ToastSpecification::Builder(
                                 vector_icons::kEmailIcon, /*body_string_id=*/0)
                                 .Build());

  // Even though we are registering a slightly different toast, the
  // ToastRegistry should still hit a CHECK because we are using an already
  // registered ToastId.
  EXPECT_DEATH(toast_registry->RegisterToast(
                   ToastId::kImageCopied,
                   ToastSpecification::Builder(vector_icons::kEmailIcon,
                                               /*body_string_id=*/0)
                       .AddCloseButton()
                       .Build()),
               "");
}

TEST_F(ToastRegistryTest, RetrieveUnregisteredToastId) {
  std::unique_ptr<ToastRegistry> toast_registry =
      std::make_unique<ToastRegistry>();
  ASSERT_TRUE(toast_registry->IsEmpty());

  // The ToastRegistry should hit a CHECK when we try to retrieve the
  // ToastSpecification for an id that was not registered.
  EXPECT_DEATH(toast_registry->GetToastSpecification(ToastId::kLinkCopied), "");
}
