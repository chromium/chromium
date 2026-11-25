// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_internals/aim_eligibility/aim_eligibility_page_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/omnibox_internals/aim_eligibility/aim_eligibility.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);
}

// Stands in for the WebUI page and records the Drive status it is sent.
class FakePage : public aim_eligibility::mojom::Page {
 public:
  FakePage() = default;
  ~FakePage() override = default;

  mojo::PendingRemote<aim_eligibility::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  aim_eligibility::mojom::DriveStatusPtr WaitForDriveStatus() {
    return drive_status_future_.Take();
  }

  // aim_eligibility::mojom::Page:
  void OnEligibilityStateChanged(
      aim_eligibility::mojom::EligibilityStatePtr state) override {}
  void OnDriveStatusChanged(
      aim_eligibility::mojom::DriveStatusPtr status) override {
    drive_status_future_.SetValue(std::move(status));
  }

 private:
  base::test::TestFuture<aim_eligibility::mojom::DriveStatusPtr>
      drive_status_future_;
  mojo::Receiver<aim_eligibility::mojom::Page> receiver_{this};
};

}  // namespace

class AimEligibilityPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    incognito_profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(
                AimEligibilityServiceFactory::GetInstance(),
                base::BindRepeating(&BuildMockAimEligibilityService))
            .BuildIncognito(profile_.get());
  }

  void TearDown() override {
    incognito_profile_ = nullptr;
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TestingProfile> incognito_profile_ = nullptr;
};

// Regression test: there is no `IdentityManager` for Incognito profiles, which
// used to crash the handler on construction. Without one, the Drive disclaimer
// status cannot be checked and is reported as restricted.
TEST_F(AimEligibilityPageHandlerTest, IncognitoReportsDisclaimerRestricted) {
  ASSERT_TRUE(incognito_profile_->IsOffTheRecord());
  ASSERT_FALSE(IdentityManagerFactory::GetForProfile(incognito_profile_));

  FakePage page;
  mojo::Remote<aim_eligibility::mojom::PageHandler> handler_remote;
  AimEligibilityPageHandler handler(incognito_profile_,
                                    handler_remote.BindNewPipeAndPassReceiver(),
                                    page.BindAndGetRemote());

  base::test::TestFuture<aim_eligibility::mojom::EligibilityStatePtr>
      state_future;
  handler.GetEligibilityState(state_future.GetCallback());
  EXPECT_TRUE(state_future.Get());

  aim_eligibility::mojom::DriveStatusPtr drive_status =
      page.WaitForDriveStatus();
  ASSERT_TRUE(drive_status);
  EXPECT_TRUE(drive_status->is_incognito);
  EXPECT_FALSE(drive_status->is_identity_match);
  EXPECT_EQ(aim_eligibility::mojom::DisclaimerState::kRestricted,
            drive_status->disclaimer_state);
  EXPECT_FALSE(drive_status->is_drive_supported);
}
