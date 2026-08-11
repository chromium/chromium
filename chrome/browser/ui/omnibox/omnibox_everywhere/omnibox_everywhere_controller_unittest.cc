// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include <set>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(GURL(""), profile, 0, true, true, true, "Test") {}
  ~TestWebUIContentsWrapper() override = default;

  void ReloadWebContents() override {}

  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};

class FakeGlobalAcceleratorListener : public ui::GlobalAcceleratorListener {
 public:
  FakeGlobalAcceleratorListener() = default;
  ~FakeGlobalAcceleratorListener() override = default;

  // ui::GlobalAcceleratorListener:
  void StartListening() override {}
  void StopListening() override {}
  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override {
    registered_accelerators_.insert(accelerator);
    return true;
  }
  void StopListeningForAccelerator(
      const ui::Accelerator& accelerator) override {
    registered_accelerators_.erase(accelerator);
  }

  bool IsRegistered(const ui::Accelerator& accelerator) const {
    return registered_accelerators_.find(accelerator) !=
           registered_accelerators_.end();
  }

 private:
  std::set<ui::Accelerator> registered_accelerators_;
};

}  // namespace

class OmniboxEverywhereControllerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    template_url_service_test_util_ =
        std::make_unique<TemplateURLServiceFactoryTestUtil>(&profile_);
    template_url_service_test_util_->VerifyLoad();
    SetDefaultSearchProvider(true);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    ChromeViewsTestBase::TearDown();
  }

  void SetDefaultSearchProvider(bool is_google) {
    TemplateURLData template_url_data;
    if (is_google) {
      template_url_data.SetShortName(u"Google");
      template_url_data.SetKeyword(u"google.com");
      template_url_data.SetURL("https://www.google.com/search?q={searchTerms}");
    } else {
      template_url_data.SetShortName(u"Other");
      template_url_data.SetKeyword(u"other.com");
      template_url_data.SetURL("https://www.other.com/search?q={searchTerms}");
    }
    auto template_url = std::make_unique<TemplateURL>(template_url_data);
    auto* template_url_ptr =
        template_url_service_test_util_->model()->Add(std::move(template_url));
    template_url_service_test_util_->model()
        ->SetUserSelectedDefaultSearchProvider(template_url_ptr);
  }

 protected:
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
  TestingProfile profile_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil>
      template_url_service_test_util_;
};

TEST_F(OmniboxEverywhereControllerTest, EnabledFeatureInstantiatesController) {
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  GlobalFeatures* features = TestingBrowserProcess::GetGlobal()->GetFeatures();
  ASSERT_TRUE(features);
  EXPECT_TRUE(features->omnibox_everywhere_controller());
}

TEST_F(OmniboxEverywhereControllerTest, OnInvokeControlsWidget) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      &profile_, GetContext());
  EXPECT_TRUE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      &profile_, GetContext());
  EXPECT_FALSE(controller.IsVisible());
}

TEST_F(OmniboxEverywhereControllerTest, NonGoogleDseBlocksOnInvoke) {
  SetDefaultSearchProvider(false);

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      &profile_, GetContext());
  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kProfilePicker,
                      &profile_);
  EXPECT_FALSE(controller.IsVisible());
}

TEST_F(OmniboxEverywhereControllerTest, HotkeyPrefDisablesHotkey) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  // Initialize with pref enabled by default.
  EXPECT_TRUE(
      local_state->GetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled));

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  ui::Accelerator hotkey(ui::VKEY_SPACE,
                         ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);

  // Controller should register the hotkey on initialization if pref is enabled.
  EXPECT_TRUE(fake_listener.IsRegistered(hotkey));

  // Disabling the pref unregisters the hotkey.
  local_state->SetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled, false);
  EXPECT_FALSE(fake_listener.IsRegistered(hotkey));

  // Re-enabling the pref registers the hotkey.
  local_state->SetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled, true);
  EXPECT_TRUE(fake_listener.IsRegistered(hotkey));
}

TEST_F(OmniboxEverywhereControllerTest, ControllerInitWithDisabledHotkeyPref) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled, false);

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  ui::Accelerator hotkey(ui::VKEY_SPACE,
                         ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);

  // Controller should NOT register the hotkey on initialization if pref is
  // disabled.
  EXPECT_FALSE(fake_listener.IsRegistered(hotkey));
}
