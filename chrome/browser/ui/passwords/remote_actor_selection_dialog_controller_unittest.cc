// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::Optional;
using testing::Eq;

const char16_t kUsername[] = u"user1";
const char16_t kPassword[] = u"password123";

PasswordForm GetLocalForm() {
  PasswordForm form;
  form.username_value = kUsername;
  form.password_value = kPassword;
  form.url = GURL("https://example.com");
  return form;
}

class RemoteActorSelectionDialogControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  RemoteActorSelectionDialogControllerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    credential_domain_ = "https://example.com";
  }

 protected:
  std::string credential_domain_;
};

TEST_F(RemoteActorSelectionDialogControllerTest, Properties) {
  std::vector<std::unique_ptr<PasswordForm>> credentials;
  credentials.push_back(std::make_unique<PasswordForm>(GetLocalForm()));

  base::MockCallback<RemoteActorSelectionDialogController::OnResultCallback>
      callback;

  EXPECT_CALL(callback, Run(Eq(std::nullopt)));

  RemoteActorSelectionDialogController controller(
      web_contents(), std::move(credentials), credential_domain_, callback.Get());

  EXPECT_EQ(controller.GetDisplayType(),
            PasswordCombinedSelectorController::DisplayType::kRemoteActor);
  EXPECT_TRUE(controller.ShouldShowTopIllustration());
  EXPECT_FALSE(controller.GetTitle().empty());
  EXPECT_FALSE(controller.GetSubtitle().empty());
  EXPECT_FALSE(controller.GetOkButtonLabel().empty());

  EXPECT_THAT(controller.GetLocalForms(), ElementsAre(Pointee(GetLocalForm())));
}

TEST_F(RemoteActorSelectionDialogControllerTest,
       ChooseCredentialsTriggersCallback) {
  std::vector<std::unique_ptr<PasswordForm>> credentials;
  PasswordForm form = GetLocalForm();
  credentials.push_back(std::make_unique<PasswordForm>(form));

  base::MockCallback<RemoteActorSelectionDialogController::OnResultCallback>
      callback;

  // We expect the callback to be called with the selected form.
  EXPECT_CALL(callback, Run(Optional(form)));

  RemoteActorSelectionDialogController controller(
      web_contents(), std::move(credentials), credential_domain_, callback.Get());

  controller.OnChooseCredentials(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(RemoteActorSelectionDialogControllerTest,
       CloseDialogTriggersCallbackWithNullopt) {
  std::vector<std::unique_ptr<PasswordForm>> credentials;
  credentials.push_back(std::make_unique<PasswordForm>(GetLocalForm()));

  base::MockCallback<RemoteActorSelectionDialogController::OnResultCallback>
      callback;

  // We expect the callback to be called with nullopt.
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));

  RemoteActorSelectionDialogController controller(
      web_contents(), std::move(credentials), credential_domain_, callback.Get());

  controller.OnCloseDialog();
}

}  // namespace

}  // namespace password_manager
