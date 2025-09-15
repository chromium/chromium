// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_handler.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom-forward.h"
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
constexpr user_education::NtpPromoIdentifier kPromo2Id = "promo 2";
constexpr user_education::NtpPromoIdentifier kPromo3Id = "promo 3";
constexpr std::string_view kPromo1Icon = "promo 1 icon";
constexpr std::string_view kPromo2Icon = "promo 2 icon";
constexpr std::string_view kPromo3Icon = "promo 3 icon";
constexpr std::string_view kPromo1Text = "promo 1 text";
constexpr std::string_view kPromo2Text = "promo 2 text";
constexpr std::string_view kPromo3Text = "promo 3 text";
constexpr std::string_view kPromo1Button = "promo 1 button";
constexpr std::string_view kPromo2Button = "promo 2 button";
constexpr std::string_view kPromo3Button = "promo 3 button";

using PromoList = std::vector<user_education::NtpShowablePromo>;

class MockClient : public ntp_promo::mojom::NtpPromoClient {
 public:
  MockClient() = default;
  ~MockClient() override = default;

  mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, SetPromos, (const PromoList&, const PromoList&));

 private:
  mojo::Receiver<ntp_promo::mojom::NtpPromoClient> receiver_{this};
};

class MockController : public user_education::NtpPromoController {
 public:
  using user_education::NtpPromoController::NtpPromoController;
  ~MockController() override = default;

  MOCK_METHOD(user_education::NtpShowablePromos,
              GenerateShowablePromos,
              (const user_education::UserEducationContextPtr&));
  MOCK_METHOD(void,
              OnPromosShown,
              (const std::vector<std::string>&,
               const std::vector<std::string>&));
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

  user_education::NtpShowablePromos GetShowablePromos() {
    user_education::NtpShowablePromos promos;
    promos.pending.emplace_back(kPromo1Id, kPromo1Icon, kPromo1Text,
                                kPromo1Button);
    promos.pending.emplace_back(kPromo3Id, kPromo3Icon, kPromo3Text,
                                kPromo3Button);
    promos.completed.emplace_back(kPromo2Id, kPromo2Icon, kPromo2Text,
                                  kPromo2Button);
    return promos;
  }

  void VerifyShowablePromos(const PromoList& eligible,
                            const PromoList& completed) {
    ASSERT_EQ(2U, eligible.size());
    EXPECT_EQ(kPromo1Id, eligible[0].id);
    EXPECT_EQ(kPromo1Icon, eligible[0].icon_name);
    EXPECT_EQ(kPromo1Text, eligible[0].body_text);
    EXPECT_EQ(kPromo1Button, eligible[0].action_button_text);
    EXPECT_EQ(kPromo3Id, eligible[1].id);
    EXPECT_EQ(kPromo3Icon, eligible[1].icon_name);
    EXPECT_EQ(kPromo3Text, eligible[1].body_text);
    EXPECT_EQ(kPromo3Button, eligible[1].action_button_text);
    ASSERT_EQ(1U, completed.size());
    EXPECT_EQ(kPromo2Id, completed[0].id);
    EXPECT_EQ(kPromo2Icon, completed[0].icon_name);
    EXPECT_EQ(kPromo2Text, completed[0].body_text);
    EXPECT_EQ(kPromo2Button, completed[0].action_button_text);
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

TEST_F(NtpPromoHandlerTest, PassesOnPromosShown) {
  EXPECT_CALL(mock_controller(),
              OnPromosShown(testing::ElementsAre(kPromo1Id, kPromo3Id),
                            testing::ElementsAre(kPromo2Id)));
  handler().OnPromosShown({kPromo1Id, kPromo3Id}, {kPromo2Id});
}

TEST_F(NtpPromoHandlerTest, RespondsToRequest) {
  EXPECT_CALL(mock_controller(), GenerateShowablePromos)
      .WillOnce(testing::Return(GetShowablePromos()));
  PromoList actual_eligible;
  PromoList actual_completed;

  // Even in tests, sending to the (mock) client is asynchronous.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_client(), SetPromos)
      .WillOnce([&](const PromoList& eligible, const PromoList& completed) {
        actual_eligible = eligible;
        actual_completed = completed;
        run_loop.Quit();
      });
  handler().RequestPromos();
  run_loop.Run();

  VerifyShowablePromos(actual_eligible, actual_completed);
}
