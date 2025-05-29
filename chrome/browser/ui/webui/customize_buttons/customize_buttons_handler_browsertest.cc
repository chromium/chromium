// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockFeaturePromoHelper : public NewTabPageFeaturePromoHelper {
 public:
  MOCK_METHOD(void,
              RecordPromoFeatureUsageAndClosePromo,
              (const base::Feature& feature, content::WebContents*),
              (override));
  MOCK_METHOD(void,
              MaybeShowFeaturePromo,
              (const base::Feature& iph_feature, content::WebContents*),
              (override));
  MOCK_METHOD(bool,
              IsSigninModalDialogOpen,
              (content::WebContents*),
              (override));

  ~MockFeaturePromoHelper() override = default;
};

class MockCustomizeButtonsDocument
    : public customize_buttons::mojom::CustomizeButtonsDocument {
 public:
  MockCustomizeButtonsDocument() = default;
  ~MockCustomizeButtonsDocument() override = default;

  mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<customize_buttons::mojom::CustomizeButtonsDocument> receiver_{
      this};

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, SetCustomizeChromeSidePanelVisibility, (bool));
};

class MockCustomizeChromeTabHelper
    : public customize_chrome::SidePanelController {
 public:
  ~MockCustomizeChromeTabHelper() override = default;

  MOCK_METHOD(bool, IsCustomizeChromeEntryAvailable, (), (const, override));
  MOCK_METHOD(bool, IsCustomizeChromeEntryShowing, (), (const, override));
  MOCK_METHOD(void,
              SetEntryChangedCallback,
              (StateChangedCallBack),
              (override));
  MOCK_METHOD(void,
              OpenSidePanel,
              (SidePanelOpenTrigger, std::optional<CustomizeChromeSection>),
              (override));
  MOCK_METHOD(void, CloseSidePanel, (), (override));

 protected:
  MOCK_METHOD(void, CreateAndRegisterEntry, (), (override));
  MOCK_METHOD(void, DeregisterEntry, (), (override));
};

class CustomizeButtonsHandlerBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    auto mock_controller_ptr = std::make_unique<MockCustomizeChromeTabHelper>();
    mock_controller_ = mock_controller_ptr.get();
    browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->SetCustomizeChromeSidePanelControllerForTesting(
            std::move(mock_controller_ptr));
  }

  void CreateHanlder(bool should_create_with_tab_interface) {
    tabs::TabInterface* tab = nullptr;
    if (should_create_with_tab_interface) {
      tab = browser()->tab_strip_model()->GetActiveTab();
    }

    auto promo_helper_ptr = std::make_unique<MockFeaturePromoHelper>();
    promo_helper_ = promo_helper_ptr.get();

    handler_ = std::make_unique<CustomizeButtonsHandler>(
        mojo::PendingReceiver<
            customize_buttons::mojom::CustomizeButtonsHandler>(),
        doc_.BindAndGetRemote(), web_ui_.get(), tab,
        std::move(promo_helper_ptr));
  }

  Profile* profile() { return browser()->profile(); }

  void TearDownOnMainThread() override {
    promo_helper_ = nullptr;
    handler_.reset();
    mock_controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  MockFeaturePromoHelper* GetMockFeaturePromoHelper() {
    return promo_helper_.get();
  }

  MockCustomizeButtonsDocument doc_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  raw_ptr<MockFeaturePromoHelper> promo_helper_;
  std::unique_ptr<CustomizeButtonsHandler> handler_;
  raw_ptr<MockCustomizeChromeTabHelper> mock_controller_;
};

class CustomizeButtonsHandlerBrowserTest
    : public CustomizeButtonsHandlerBrowserTestBase,
      public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(All,
                         CustomizeButtonsHandlerBrowserTest,
                         // Run the tests with CustomizeButtonsHandler created
                         // with and without a TabInterface.
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(CustomizeButtonsHandlerBrowserTest, OpenSidePanel) {
  CreateHanlder(GetParam());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SidePanelOpenTrigger trigger;
  std::optional<CustomizeChromeSection> section;

  EXPECT_CALL(*mock_controller_.get(), OpenSidePanel)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&trigger),
                               testing::SaveArg<1>(&section)));
  EXPECT_CALL(
      *GetMockFeaturePromoHelper(),
      RecordPromoFeatureUsageAndClosePromo(
          testing::Ref(feature_engagement::kIPHDesktopCustomizeChromeFeature),
          web_contents))
      .Times(1);
  EXPECT_CALL(
      *GetMockFeaturePromoHelper(),
      RecordPromoFeatureUsageAndClosePromo(
          testing::Ref(
              feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature),
          web_contents))
      .Times(1);

  handler_->SetCustomizeChromeSidePanelVisible(
      /*visible=*/true,
      customize_buttons::mojom::CustomizeChromeSection::kAppearance,
      customize_buttons::mojom::SidePanelOpenTrigger::kNewTabPage);

  EXPECT_EQ(SidePanelOpenTrigger::kNewTabPage, trigger);
  EXPECT_EQ(CustomizeChromeSection::kAppearance, section);
  doc_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_P(CustomizeButtonsHandlerBrowserTest, CloseSidePanel) {
  CreateHanlder(GetParam());
  ON_CALL(*mock_controller_.get(), IsCustomizeChromeEntryShowing())
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(*mock_controller_.get(), CloseSidePanel).Times(1);
  EXPECT_CALL(*GetMockFeaturePromoHelper(),
              RecordPromoFeatureUsageAndClosePromo)
      .Times(0);

  handler_->SetCustomizeChromeSidePanelVisible(
      /*visible=*/false,
      customize_buttons::mojom::CustomizeChromeSection::kModules,
      customize_buttons::mojom::SidePanelOpenTrigger::kNewTabPage);
  doc_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_P(CustomizeButtonsHandlerBrowserTest,
                       IncrementCustomizeChromeButtonOpenCount) {
  CreateHanlder(GetParam());
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            0);

  handler_->IncrementCustomizeChromeButtonOpenCount();

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            1);

  handler_->IncrementCustomizeChromeButtonOpenCount();

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            2);
}

IN_PROC_BROWSER_TEST_P(CustomizeButtonsHandlerBrowserTest,
                       IncrementWallpaperSearchButtonShownCount) {
  CreateHanlder(GetParam());
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpWallpaperSearchButtonShownCount),
            0);
  handler_->IncrementWallpaperSearchButtonShownCount();

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kNtpWallpaperSearchButtonShownCount),
            1);
}

class CustomizeButtonsHandlerBrowserTestWithParam
    : public CustomizeButtonsHandlerBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<customize_buttons::mojom::CustomizeChromeSection,
                     customize_buttons::mojom::SidePanelOpenTrigger>> {
 protected:
  customize_buttons::mojom::CustomizeChromeSection GetSectionParam() {
    return std::get<0>(GetParam());
  }

  customize_buttons::mojom::SidePanelOpenTrigger GetTriggerParam() {
    return std::get<1>(GetParam());
  }

  CustomizeChromeSection GetExpectedSection(
      customize_buttons::mojom::CustomizeChromeSection section) {
    // TODO(crbug.com/419081665) Dedupe CustomizeChromeSection mojom enums.
    switch (section) {
      case customize_buttons::mojom::CustomizeChromeSection::kUnspecified:
        return CustomizeChromeSection::kUnspecified;
      case customize_buttons::mojom::CustomizeChromeSection::kAppearance:
        return CustomizeChromeSection::kAppearance;
      case customize_buttons::mojom::CustomizeChromeSection::kShortcuts:
        return CustomizeChromeSection::kShortcuts;
      case customize_buttons::mojom::CustomizeChromeSection::kModules:
        return CustomizeChromeSection::kModules;
      case customize_buttons::mojom::CustomizeChromeSection::kWallpaperSearch:
        return CustomizeChromeSection::kWallpaperSearch;
      case customize_buttons::mojom::CustomizeChromeSection::kToolbar:
        return CustomizeChromeSection::kToolbar;
    }
  }

  SidePanelOpenTrigger GetExpectedTrigger(
      customize_buttons::mojom::SidePanelOpenTrigger trigger) {
    switch (trigger) {
      case customize_buttons::mojom::SidePanelOpenTrigger::kNewTabPage:
        return SidePanelOpenTrigger::kNewTabPage;
      case customize_buttons::mojom::SidePanelOpenTrigger::kNewTabFooter:
        return SidePanelOpenTrigger::kNewTabFooter;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CustomizeButtonsHandlerBrowserTestWithParam,
    testing::Combine(
        testing::Values(
            customize_buttons::mojom::CustomizeChromeSection::kUnspecified,
            customize_buttons::mojom::CustomizeChromeSection::kAppearance,
            customize_buttons::mojom::CustomizeChromeSection::kShortcuts,
            customize_buttons::mojom::CustomizeChromeSection::kModules,
            customize_buttons::mojom::CustomizeChromeSection::kWallpaperSearch,
            customize_buttons::mojom::CustomizeChromeSection::kToolbar),
        testing::Values(
            customize_buttons::mojom::SidePanelOpenTrigger::kNewTabPage,
            customize_buttons::mojom::SidePanelOpenTrigger::kNewTabFooter)));

IN_PROC_BROWSER_TEST_P(CustomizeButtonsHandlerBrowserTestWithParam,
                       OpenSidePanel) {
  CreateHanlder(false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::optional<CustomizeChromeSection> section;
  SidePanelOpenTrigger trigger;

  EXPECT_CALL(*mock_controller_.get(), OpenSidePanel)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&trigger),
                               testing::SaveArg<1>(&section)));
  EXPECT_CALL(
      *GetMockFeaturePromoHelper(),
      RecordPromoFeatureUsageAndClosePromo(
          testing::Ref(feature_engagement::kIPHDesktopCustomizeChromeFeature),
          web_contents))
      .Times(1);
  EXPECT_CALL(
      *GetMockFeaturePromoHelper(),
      RecordPromoFeatureUsageAndClosePromo(
          testing::Ref(
              feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature),
          web_contents))
      .Times(1);

  const customize_buttons::mojom::CustomizeChromeSection sectionParam =
      GetSectionParam();
  const customize_buttons::mojom::SidePanelOpenTrigger triggerParam =
      GetTriggerParam();
  handler_->SetCustomizeChromeSidePanelVisible(/*visible=*/true, sectionParam,
                                               triggerParam);

  EXPECT_EQ(section.value(), GetExpectedSection(sectionParam));
  EXPECT_EQ(trigger, GetExpectedTrigger(triggerParam));
  doc_.FlushForTesting();
}
