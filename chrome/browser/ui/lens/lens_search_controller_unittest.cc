// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/test/base/testing_profile.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/rect_f.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace lens {

class LensSearchControllerTest : public testing::Test {
 public:
  LensSearchControllerTest() = default;
  ~LensSearchControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {lens::features::kLensOverlayNonBlockingPrivacyNotice,
         lens::features::kLensOverlayNonBlockingPrivacyNoticeForImageSearch},
        {});
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), content::SiteInstance::Create(profile_.get()));

    mock_browser_window_interface_ =
        std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(mock_browser_window_interface_.get()));
    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(mock_tab_interface_, IsActivated()).WillByDefault(Return(true));

    side_panel_registry_ =
        std::make_unique<SidePanelRegistry>(&mock_tab_interface_);

    controller_ =
        std::make_unique<TestLensSearchController>(&mock_tab_interface_);

    auto* theme_service = ThemeServiceFactory::GetForProfile(profile_.get());
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    auto* sync_service = SyncServiceFactory::GetForProfile(profile_.get());
    controller_->Initialize(nullptr, identity_manager, profile_->GetPrefs(),
                            sync_service, theme_service);
  }

  void TearDown() override {
    if (controller_ && !controller_->IsOff()) {
      controller_->CloseLensSync(
          lens::LensOverlayDismissalSource::kOverlayCloseButton);
    }
    controller_.reset();
    side_panel_registry_.reset();
    mock_browser_window_interface_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_interface_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<TestLensSearchController> controller_;
};

TEST_F(LensSearchControllerTest,
       OpenLensOverlayWithPendingRegion_AllowsNonBlockingPrivacyNotice) {
  profile_->GetPrefs()->SetBoolean(
      lens::prefs::kLensSharingPageScreenshotEnabled, false);
  profile_->GetPrefs()->SetBoolean(lens::prefs::kLensSharingPageContentEnabled,
                                   false);

  ASSERT_TRUE(controller_->IsOff());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(0.1, 0.1, 0.2, 0.2);

  controller_->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      std::move(region), bitmap);

  EXPECT_FALSE(controller_->IsOff());
}

}  // namespace lens
