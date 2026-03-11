// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_handler.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/test/mock_user_education_context.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr user_education::NtpPromoIdentifier kPromo1Id = "promo 1";
constexpr std::string_view kPromo1Icon = "promo 1 icon";
constexpr std::string_view kPromo1Text = "promo 1 text";
constexpr std::string_view kPromo1Button = "promo 1 button";

class MockClient : public ntp_promo::mojom::NtpPromoClient {
 public:
  MockClient() = default;
  ~MockClient() override = default;

  mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SetPromo,
              (const std::optional<user_education::NtpShowablePromo>&));

 private:
  mojo::Receiver<ntp_promo::mojom::NtpPromoClient> receiver_{this};
};

class MockController : public user_education::NtpPromoController {
 public:
  using user_education::NtpPromoController::NtpPromoController;
  ~MockController() override = default;

  MOCK_METHOD(user_education::NtpShowablePromos,
              GenerateShowablePromo,
              (const user_education::UserEducationContextPtr&));
  MOCK_METHOD(void, OnPromoShown, (const user_education::NtpPromoIdentifier&));
  MOCK_METHOD(void,
              OnPromoClicked,
              (user_education::NtpPromoIdentifier,
               const user_education::UserEducationContextPtr&));
};

}  // namespace

class NtpPromoHandlerTest : public testing::Test {
 public:
  NtpPromoHandlerTest() = default;
  ~NtpPromoHandlerTest() override = default;

  MockController& mock_controller() { return mock_controller_; }
  MockClient& mock_client() { return mock_client_; }
  NtpPromoHandler& handler() { return *handler_; }
  const scoped_refptr<user_education::UserEducationContext>& mock_context() {
    return mock_context_;
  }

  user_education::NtpShowablePromos GetShowablePromo() {
    user_education::NtpShowablePromos promos;
    promos.promo = user_education::NtpShowablePromo(kPromo1Id, kPromo1Icon,
                                                    kPromo1Text, kPromo1Button);
    return promos;
  }

  void VerifyShowablePromo(
      const std::optional<user_education::NtpShowablePromo>& promo) {
    ASSERT_TRUE(promo.has_value());
    EXPECT_EQ(kPromo1Id, promo->id);
    EXPECT_EQ(kPromo1Icon, promo->icon_name);
    EXPECT_EQ(kPromo1Text, promo->body_text);
    EXPECT_EQ(kPromo1Button, promo->action_button_text);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;
  user_education::NtpPromoRegistry promo_registry_;
  user_education::test::TestUserEducationStorageService storage_service_;
  MockController mock_controller_{promo_registry_, storage_service_,
                                  user_education::NtpPromoControllerParams()};
  MockClient mock_client_;
  scoped_refptr<user_education::UserEducationContext> mock_context_ =
      base::MakeRefCounted<user_education::test::MockUserEducationContext>();
  std::unique_ptr<NtpPromoHandler> handler_ = NtpPromoHandler::CreateForTesting(
      mock_client_.BindAndGetRemote(),
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler>(),
      mock_context(),
      &mock_controller_);
};

TEST_F(NtpPromoHandlerTest, PassesOnClick) {
  EXPECT_CALL(mock_controller(), OnPromoClicked(kPromo1Id, mock_context()));
  handler().OnPromoClicked(kPromo1Id);
}

TEST_F(NtpPromoHandlerTest, PassesOnPromoShown) {
  EXPECT_CALL(mock_controller(), OnPromoShown(kPromo1Id));
  handler().OnPromoShown(kPromo1Id);
}

TEST_F(NtpPromoHandlerTest, RespondsToRequest) {
  EXPECT_CALL(mock_controller(), GenerateShowablePromo)
      .WillOnce(testing::Return(GetShowablePromo()));
  std::optional<user_education::NtpShowablePromo> actual_promo;

  // Even in tests, sending to the (mock) client is asynchronous.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_client(), SetPromo)
      .WillOnce(
          [&](const std::optional<user_education::NtpShowablePromo>& promo) {
            actual_promo = promo;
            run_loop.Quit();
          });
  handler().RequestPromos();
  run_loop.Run();

  VerifyShowablePromo(actual_promo);
}
