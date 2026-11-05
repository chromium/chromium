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
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::Optional;
using testing::Pointee;

const char16_t kUsername[] = u"user1";
const char16_t kPassword[] = u"password123";

PasswordForm GetLocalForm() {
  PasswordForm form;
  form.username_value = kUsername;
  form.password_value = PasswordString(std::u16string(kPassword));
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
      web_contents(), std::move(credentials), credential_domain_,
      callback.Get());

  EXPECT_EQ(controller.GetDisplayType(),
            PasswordCombinedSelectorController::DisplayType::kRemoteActor);
  EXPECT_TRUE(controller.ShouldShowTopIllustration());
  EXPECT_EQ(controller.GetActorType(), RemoteActorType::kGemini);
  EXPECT_TRUE(controller.GetActorIconUrl().is_empty());
  const GURL custom_icon_url("https://www.gstatic.com/images/icon.png");
  controller.SetActorIconUrl(custom_icon_url);
  EXPECT_EQ(controller.GetActorIconUrl(), custom_icon_url);
  controller.SetActorIconUrl(GURL("http://www.gstatic.com/images/icon.png"));
  EXPECT_TRUE(controller.GetActorIconUrl().is_empty());
  controller.SetActorIconUrl(GURL("not-a-valid-url"));
  EXPECT_TRUE(controller.GetActorIconUrl().is_empty());
  EXPECT_EQ(controller.GetTitle(),
            u"Allow Gemini to sign in to example.com for you?");
  EXPECT_EQ(controller.GetSubtitle(),
            u"Gemini can use Google Password Manager to sign in for you. Learn "
            u"how Gemini handles your data");
  gfx::Range link_range = controller.GetSubtitleLinkRange();
  ASSERT_FALSE(link_range.is_empty());
  EXPECT_EQ(controller.GetSubtitle().substr(link_range.start(),
                                            link_range.length()),
            u"Learn how Gemini handles your data");
  EXPECT_EQ(controller.GetOkButtonLabel(), u"Allow this time");

  EXPECT_THAT(controller.GetLocalForms(), ElementsAre(Pointee(GetLocalForm())));
}

TEST_F(RemoteActorSelectionDialogControllerTest,
       SetActorNameAndLearnMoreUrlOverrideDefaults) {
  std::vector<std::unique_ptr<PasswordForm>> credentials;
  credentials.push_back(std::make_unique<PasswordForm>(GetLocalForm()));

  base::MockCallback<RemoteActorSelectionDialogController::OnResultCallback>
      callback;
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));

  RemoteActorSelectionDialogController controller(
      web_contents(), std::move(credentials), credential_domain_,
      callback.Get());

  EXPECT_EQ(controller.GetLearnMoreUrl(),
            GURL("https://support.google.com/gemini?p=lm_gpm_data"));
  EXPECT_FALSE(controller.GetSubtitleLinkRange().is_empty());

  controller.SetActorType(RemoteActorType::kSearch);
  EXPECT_EQ(controller.GetActorType(), RemoteActorType::kSearch);
  EXPECT_EQ(controller.GetTitle(),
            u"Allow Search Agent to sign in to example.com for you?");
  gfx::Range link_range = controller.GetSubtitleLinkRange();
  ASSERT_FALSE(link_range.is_empty());
  EXPECT_EQ(controller.GetSubtitle(),
            u"Search Agent can use Google Password Manager to sign in for you. "
            u"Learn how Search Agent handles your data");
  EXPECT_EQ(
      controller.GetSubtitle().substr(link_range.start(), link_range.length()),
      u"Learn how Search Agent handles your data");

  controller.SetActorName(u"Custom Search Agent");
  controller.SetLearnMoreUrl(GURL("https://g.co/privacypolicy"));
  EXPECT_EQ(controller.GetTitle(),
            u"Allow Custom Search Agent to sign in to example.com for you?");
  link_range = controller.GetSubtitleLinkRange();
  ASSERT_FALSE(link_range.is_empty());
  EXPECT_EQ(controller.GetSubtitle(),
            u"Custom Search Agent can use Google Password Manager to sign in "
            u"for you. Learn how Custom Search Agent handles your data");
  EXPECT_EQ(
      controller.GetSubtitle().substr(link_range.start(), link_range.length()),
      u"Learn how Custom Search Agent handles your data");
  EXPECT_EQ(controller.GetLearnMoreUrl(), GURL("https://g.co/privacypolicy"));

  // Empty actor name falls back to the default for `actor_type_`.
  controller.SetActorName(u"");
  EXPECT_EQ(controller.GetTitle(),
            u"Allow Search Agent to sign in to example.com for you?");

  // Non-HTTPS URLs must be rejected and fall back to the default help URL.
  controller.SetLearnMoreUrl(GURL("http://insecure.example.com"));
  EXPECT_EQ(controller.GetLearnMoreUrl(),
            GURL("https://support.google.com/gemini?p=lm_gpm_data"));
}

TEST_F(RemoteActorSelectionDialogControllerTest,
       SubtitleLinkClickedOpensHelpUrl) {
  std::vector<std::unique_ptr<PasswordForm>> credentials;
  credentials.push_back(std::make_unique<PasswordForm>(GetLocalForm()));

  base::MockCallback<RemoteActorSelectionDialogController::OnResultCallback>
      callback;

  RemoteActorSelectionDialogController controller(
      web_contents(), std::move(credentials), credential_domain_,
      callback.Get());

  gfx::Range range = controller.GetSubtitleLinkRange();
  EXPECT_FALSE(range.is_empty());
  EXPECT_LT(range.start(), range.end());

  controller.OnSubtitleLinkClicked();
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
      web_contents(), std::move(credentials), credential_domain_,
      callback.Get());

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
      web_contents(), std::move(credentials), credential_domain_,
      callback.Get());

  controller.OnCloseDialog();
}

}  // namespace

}  // namespace password_manager
