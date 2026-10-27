// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/content/origin_gating_tab_controller.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/gtest_util.h"
#include "components/origin_gating/core/checker_id.h"
#include "components/origin_gating/core/gateable_event.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_registration.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace origin_gating {
namespace {

using ::testing::NiceMock;

class MockDelegate : public OriginGatingChecker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DoesOriginRequireUserConfirmation,
              (GatingDecisionContext * context,
               const GateableEvent& event,
               DoesOriginRequireUserConfirmationCallback callback),
              (const, override));
  MOCK_METHOD(void,
              EvaluateEnterprisePolicy,
              (const GURL& destination,
               EvaluateEnterprisePolicyCallback callback),
              (const, override));
  MOCK_METHOD(void,
              OnNoVerdict,
              (GatingDecisionContext * context,
               const GateableEvent& event,
               bool requires_user_confirmation,
               base::OnceCallback<void(NoVerdictResult)> callback),
              (override));

  base::WeakPtr<MockDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class OriginGatingTabControllerTest
    : public content::RenderViewHostTestHarness {
 protected:
  std::unique_ptr<OriginGatingRegistration> RegisterChecker() {
    return service_->CreateAndRegisterChecker(
        delegate_.GetWeakPtr(),
        OriginGatingConfiguration(
            {}, OriginGatingConfiguration::CacheScope::kOrigin));
  }

  void SimulateOpenRequestedUrl(content::WebContentsObserver* observer,
                                content::WebContents* new_contents) {
    observer->DidOpenRequestedURL(
        new_contents, main_rfh(), GURL("https://example.com/"),
        content::Referrer(),
        ui::mojom::WindowOpenDisposition::kNewForegroundTab,
        ui::PAGE_TRANSITION_LINK, /*started_from_context_menu=*/false,
        /*renderer_initiated=*/true);
  }

  NiceMock<MockDelegate> delegate_;
  std::unique_ptr<OriginGatingService> service_ =
      OriginGatingService::CreateForTesting();
};

TEST_F(OriginGatingTabControllerTest, DefaultStateHasNullChecker) {
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->GetChecker(), nullptr);
}

TEST_F(OriginGatingTabControllerTest, SetCheckerId) {
  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  OriginGatingChecker* expected_checker =
      service_->GetChecker(registration->id());
  ASSERT_TRUE(expected_checker);

  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration->id());
  EXPECT_EQ(controller->GetChecker(), expected_checker);
}

TEST_F(OriginGatingTabControllerTest, SetCheckerIdIsIdempotent) {
  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  OriginGatingChecker* expected_checker =
      service_->GetChecker(registration->id());
  ASSERT_TRUE(expected_checker);

  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration->id());
  controller->SetCheckerId(registration->id());
  EXPECT_EQ(controller->GetChecker(), expected_checker);
}

TEST_F(OriginGatingTabControllerTest, SetCheckerIdIgnoresInactiveCheckerId) {
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);

  // Calling SetCheckerId with an ID that has no live checker in `service_`
  // should not mutate `checker_id_`. If it did store the unregistered ID (1),
  // registering the first checker below (which receives ID 1) would cause
  // `GetChecker()` to return a non-null checker.
  controller->SetCheckerId(CheckerId::FromUnsafeValue(1));
  EXPECT_EQ(controller->GetChecker(), nullptr);

  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  ASSERT_EQ(registration->id(), CheckerId::FromUnsafeValue(1));
  EXPECT_EQ(controller->GetChecker(), nullptr);
}

TEST_F(OriginGatingTabControllerTest, SetCheckerIdCrashesOnNullCheckerId) {
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  EXPECT_CHECK_DEATH(controller->SetCheckerId(CheckerId()));
}

TEST_F(OriginGatingTabControllerTest,
       SetCheckerIdDoesNotOverwriteActiveChecker) {
  std::unique_ptr<OriginGatingRegistration> registration1 = RegisterChecker();
  std::unique_ptr<OriginGatingRegistration> registration2 = RegisterChecker();
  OriginGatingChecker* expected_checker1 =
      service_->GetChecker(registration1->id());
  ASSERT_TRUE(expected_checker1);

  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration1->id());

  controller->SetCheckerId(registration2->id());
  EXPECT_EQ(controller->GetChecker(), expected_checker1);
}

TEST_F(OriginGatingTabControllerTest, ClearCheckerId) {
  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration->id());
  ASSERT_NE(controller->GetChecker(), nullptr);

  controller->ClearCheckerId();
  EXPECT_EQ(controller->GetChecker(), nullptr);
}

TEST_F(OriginGatingTabControllerTest,
       ClearCheckerIdAllowsReapplyingOrSettingNewChecker) {
  std::unique_ptr<OriginGatingRegistration> registration1 = RegisterChecker();
  std::unique_ptr<OriginGatingRegistration> registration2 = RegisterChecker();
  OriginGatingChecker* expected_checker2 =
      service_->GetChecker(registration2->id());
  ASSERT_TRUE(expected_checker2);

  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration1->id());
  controller->ClearCheckerId();

  controller->SetCheckerId(registration2->id());
  EXPECT_EQ(controller->GetChecker(), expected_checker2);
}

TEST_F(OriginGatingTabControllerTest,
       GetCheckerReturnsNullAfterRegistrationDestroyed) {
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);

  {
    std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
    controller->SetCheckerId(registration->id());
    ASSERT_NE(controller->GetChecker(), nullptr);
  }

  EXPECT_EQ(controller->GetChecker(), nullptr);
}

TEST_F(OriginGatingTabControllerTest,
       SetCheckerIdSucceedsAfterPreviousRegistrationDestroyed) {
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);

  {
    std::unique_ptr<OriginGatingRegistration> registration1 = RegisterChecker();
    controller->SetCheckerId(registration1->id());
  }

  std::unique_ptr<OriginGatingRegistration> registration2 = RegisterChecker();
  OriginGatingChecker* expected_checker2 =
      service_->GetChecker(registration2->id());
  controller->SetCheckerId(registration2->id());
  EXPECT_EQ(controller->GetChecker(), expected_checker2);
}

TEST_F(OriginGatingTabControllerTest,
       GetCheckerReturnsNullAfterServiceShutdown) {
  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration->id());
  ASSERT_NE(controller->GetChecker(), nullptr);

  service_->Shutdown();
  EXPECT_EQ(controller->GetChecker(), nullptr);
}

TEST_F(OriginGatingTabControllerTest,
       DidOpenRequestedUrlPropagatesActiveCheckerToChildWebContents) {
  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration->id());

  std::unique_ptr<content::WebContents> child_contents =
      CreateTestWebContents();
  SimulateOpenRequestedUrl(controller, child_contents.get());

  auto* child_controller =
      OriginGatingTabController::FromWebContents(child_contents.get());
  ASSERT_TRUE(child_controller);
  EXPECT_EQ(child_controller->GetChecker(), controller->GetChecker());
}

TEST_F(OriginGatingTabControllerTest,
       DidOpenRequestedUrlDoesNotPropagateWhenCheckerInactive) {
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);

  {
    std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
    controller->SetCheckerId(registration->id());
  }
  ASSERT_EQ(controller->GetChecker(), nullptr);

  std::unique_ptr<content::WebContents> child_contents =
      CreateTestWebContents();
  SimulateOpenRequestedUrl(controller, child_contents.get());

  EXPECT_EQ(OriginGatingTabController::FromWebContents(child_contents.get()),
            nullptr);
}

TEST_F(OriginGatingTabControllerTest,
       AboutToBeDiscardedTransfersCheckerToReplacementWebContents) {
  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  OriginGatingChecker* expected_checker =
      service_->GetChecker(registration->id());
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      web_contents(), *service_);
  controller->SetCheckerId(registration->id());

  std::unique_ptr<content::WebContents> replacement_contents =
      CreateTestWebContents();
  web_contents()->AboutToBeDiscarded(replacement_contents.get());

  auto* replacement_controller =
      OriginGatingTabController::FromWebContents(replacement_contents.get());
  ASSERT_TRUE(replacement_controller);
  EXPECT_EQ(replacement_controller->GetChecker(), expected_checker);
}

}  // namespace
}  // namespace origin_gating
