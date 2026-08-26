// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include <set>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_activation_waiter.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#endif

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
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEnabled, true);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
        omnibox_everywhere::prefs::kHotkeyEnabled, true);
    template_url_service_test_util_ =
        std::make_unique<TemplateURLServiceFactoryTestUtil>(profile_.get());
    template_url_service_test_util_->VerifyLoad();
    SetDefaultSearchProvider(true);
  }

  void TearDown() override {
    template_url_service_test_util_.reset();
    profile_.reset();
    if (TestingBrowserProcess::GetGlobal()->GetFeatures()) {
      TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    }
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
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil>
      template_url_service_test_util_;
};

using OmniboxEverywhereGlobalFeaturesTest = ChromeViewsTestBase;

TEST_F(OmniboxEverywhereGlobalFeaturesTest,
       EnabledFeatureInstantiatesController) {
  base::test::ScopedFeatureList feature_list{omnibox::kOmniboxEverywhere};
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  GlobalFeatures* features = TestingBrowserProcess::GetGlobal()->GetFeatures();
  ASSERT_TRUE(features);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  EXPECT_TRUE(features->omnibox_everywhere_controller());
#else
  EXPECT_FALSE(features->omnibox_everywhere_controller());
#endif

  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
}

// Tests that invoking in ephemeral mode toggles between visible and hidden.
TEST_F(OmniboxEverywhereControllerTest, OnInvokeEphemeralModeToggling) {
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
}

// Tests that invoking in persistent mode toggles between floating active and
// normal demoted.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OnInvokePersistentModeToggling OnInvokePersistentModeToggling
#else
#define MAYBE_OnInvokePersistentModeToggling \
  DISABLED_OnInvokePersistentModeToggling
#endif
TEST_F(OmniboxEverywhereControllerTest, MAYBE_OnInvokePersistentModeToggling) {
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  EXPECT_FALSE(controller.IsVisible());

  // Initial toggle: Shows widget and activates.
  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
  views::Widget* widget = controller.ui_manager()->widget();
  ASSERT_TRUE(widget);
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(controller.ui_manager()->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Second toggle while active: Demotes widget and deactivates,
  // keeping IsVisible() == true.
  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
  EXPECT_FALSE(controller.ui_manager()->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Third toggle while demoted/inactive: Re-activates widget.
  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(controller.ui_manager()->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);
}

// Tests that calling Hide() directly on the controller deactivates the widget
// while keeping it visible in persistent mode.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HidePersistentMode HidePersistentMode
#else
#define MAYBE_HidePersistentMode DISABLED_HidePersistentMode
#endif
TEST_F(OmniboxEverywhereControllerTest, MAYBE_HidePersistentMode) {
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
  views::Widget* widget = controller.ui_manager()->widget();
  ASSERT_TRUE(widget);
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(controller.ui_manager()->IsActive());

  controller.Hide();
  EXPECT_TRUE(controller.IsVisible());
  EXPECT_FALSE(controller.ui_manager()->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);
}

// Tests that calling Hide() directly on the controller closes the widget
// in ephemeral mode.
TEST_F(OmniboxEverywhereControllerTest, HideEphemeralMode) {
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
  views::Widget* widget = controller.ui_manager()->widget();
  ASSERT_TRUE(widget);
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(controller.ui_manager()->IsActive());

  controller.Hide();
  EXPECT_FALSE(controller.IsVisible());
}

TEST_F(OmniboxEverywhereControllerTest, NonGoogleDseBlocksOnInvoke) {
  SetDefaultSearchProvider(false);

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kProfilePicker,
                      profile_.get());
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

  ui::Accelerator hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);

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

  ui::Accelerator hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);

  // Controller should NOT register the hotkey on initialization if pref is
  // disabled.
  EXPECT_FALSE(fake_listener.IsRegistered(hotkey));
}

TEST_F(OmniboxEverywhereControllerTest,
       ResolvesOffTheRecordProfileToOriginalProfile) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  // SetTargetProfile with an off-the-record profile resolves to its original
  // profile.
  controller.SetTargetProfile(otr_profile);
  EXPECT_EQ(profile_.get(), controller.target_profile());

  // Activating an off-the-record browser window also resolves to its original
  // profile.
  MockBrowserWindowInterface otr_bwi;
  EXPECT_CALL(otr_bwi, GetProfile())
      .WillRepeatedly(testing::Return(otr_profile));

  controller.OnBrowserActivated(&otr_bwi);
  EXPECT_EQ(profile_.get(), controller.target_profile());
}

TEST_F(OmniboxEverywhereControllerTest, TargetProfilePersistedToPref) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Setting target profile should update prefs::kLastTargetProfileDir.
  controller.SetTargetProfile(profile_.get());
  EXPECT_EQ(local_state->GetFilePath(
                omnibox_everywhere::prefs::kLastTargetProfileDir),
            profile_->GetPath());
}

TEST_F(OmniboxEverywhereControllerTest, RestoresTargetProfileOnStartup) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetFilePath(omnibox_everywhere::prefs::kLastTargetProfileDir,
                           profile_->GetPath());

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Simulating OnProfileAdded with matching profile path should restore it as
  // target_profile().
  controller.OnProfileAdded(profile_.get());
  EXPECT_EQ(profile_.get(), controller.target_profile());
  EXPECT_EQ(local_state->GetFilePath(
                omnibox_everywhere::prefs::kLastTargetProfileDir),
            profile_->GetPath());
}

TEST_F(OmniboxEverywhereControllerTest,
       RestoresMatchingPersistedProfileWhenMultipleProfilesAdded) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  TestingProfile profile1;

  // Persist profile_'s path as the target profile.
  local_state->SetFilePath(omnibox_everywhere::prefs::kLastTargetProfileDir,
                           profile_->GetPath());

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Adding profile1 (non-matching) should NOT set it as target profile.
  controller.OnProfileAdded(&profile1);
  EXPECT_NE(&profile1, controller.target_profile());
  EXPECT_EQ(nullptr, controller.target_profile());

  // Adding profile_ (matching eligible profile) should restore it as target
  // profile.
  controller.OnProfileAdded(profile_.get());
  EXPECT_EQ(profile_.get(), controller.target_profile());
}

TEST_F(OmniboxEverywhereControllerTest,
       IgnoresOffTheRecordProfileOnProfileAdded) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  // OnProfileAdded ignores off-the-record profile.
  controller.OnProfileAdded(otr_profile);
  EXPECT_EQ(nullptr, controller.target_profile());
  EXPECT_TRUE(
      local_state->GetFilePath(omnibox_everywhere::prefs::kLastTargetProfileDir)
          .empty());

  // Normal regular profile is accepted when added.
  controller.OnProfileAdded(profile_.get());
  EXPECT_EQ(profile_.get(), controller.target_profile());
}

TEST_F(OmniboxEverywhereControllerTest,
       IneligibleProfileInSetTargetProfileIsNoOp) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Initially set an eligible target profile.
  controller.SetTargetProfile(profile_.get());
  EXPECT_EQ(profile_.get(), controller.target_profile());

  // Passing an ineligible profile (e.g. missing service) is a NO-OP.
  TestingProfile ineligible_profile;
  controller.SetTargetProfile(&ineligible_profile);
  EXPECT_EQ(profile_.get(), controller.target_profile());

  // Explicitly passing nullptr clears target profile to nullptr.
  controller.SetTargetProfile(nullptr);
  EXPECT_EQ(nullptr, controller.target_profile());
}

TEST_F(OmniboxEverywhereControllerTest,
       TargetProfileUpdatesOnBrowserActivation) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  TestingProfile profile2;
  TemplateURLServiceFactoryTestUtil util2(&profile2);
  util2.VerifyLoad();
  TemplateURLData data;
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  util2.model()->SetUserSelectedDefaultSearchProvider(
      util2.model()->Add(std::make_unique<TemplateURL>(data)));

  MockBrowserWindowInterface bwi1;
  EXPECT_CALL(bwi1, GetProfile())
      .WillRepeatedly(testing::Return(profile_.get()));

  MockBrowserWindowInterface bwi2;
  EXPECT_CALL(bwi2, GetProfile()).WillRepeatedly(testing::Return(&profile2));

  // Activating bwi1 sets target profile to profile_.
  controller.OnBrowserActivated(&bwi1);
  EXPECT_EQ(profile_.get(), controller.target_profile());

  // Activating bwi2 updates target profile to profile2.
  controller.OnBrowserActivated(&bwi2);
  EXPECT_EQ(&profile2, controller.target_profile());

  // Re-activating bwi1 updates target profile back to profile_.
  controller.OnBrowserActivated(&bwi1);
  EXPECT_EQ(profile_.get(), controller.target_profile());
}

TEST_F(OmniboxEverywhereControllerTest, ControllerUpdatesCustomHotkey) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  ui::Accelerator default_hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  EXPECT_TRUE(fake_listener.IsRegistered(default_hotkey));

  ui::Accelerator new_hotkey(ui::VKEY_SPACE,
                             ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  local_state->SetString(omnibox_everywhere::prefs::kOmniboxEverywhereHotkey,
                         "Ctrl+Shift+Space");

  EXPECT_FALSE(fake_listener.IsRegistered(default_hotkey));
  EXPECT_TRUE(fake_listener.IsRegistered(new_hotkey));
}
TEST_F(OmniboxEverywhereControllerTest,
       ControllerUpdatesCustomHotkeyWhileSuspended) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  ui::Accelerator default_hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  EXPECT_TRUE(fake_listener.IsRegistered(default_hotkey));

  // Simulate WebUI <cr-shortcut-input> setting shortcut suspension during key
  // capture so key combinations aren't intercepted globally while typing.
  fake_listener.SetShortcutHandlingSuspended(true);
  EXPECT_FALSE(fake_listener.IsRegistered(default_hotkey));

  ui::Accelerator new_hotkey(ui::VKEY_SPACE,
                             ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  local_state->SetString(omnibox_everywhere::prefs::kOmniboxEverywhereHotkey,
                         "Ctrl+Shift+Space");

  // While suspended, the platform listener should not be actively listening for
  // any hotkeys so keyboard events are not intercepted.
  EXPECT_TRUE(fake_listener.IsShortcutHandlingSuspended());
  EXPECT_FALSE(fake_listener.IsRegistered(default_hotkey));
  EXPECT_FALSE(fake_listener.IsRegistered(new_hotkey));

  // When key capture finishes and suspension ends, the new hotkey should be
  // actively registered with the platform listener and the old one should not.
  fake_listener.SetShortcutHandlingSuspended(false);
  EXPECT_FALSE(fake_listener.IsRegistered(default_hotkey));
  EXPECT_TRUE(fake_listener.IsRegistered(new_hotkey));
}

TEST_F(OmniboxEverywhereControllerTest, ControllerLoadsCustomHotkeyOnStartup) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetString(omnibox_everywhere::prefs::kOmniboxEverywhereHotkey,
                         "Ctrl+Shift+Space");

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  ui::Accelerator default_hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  ui::Accelerator custom_hotkey(ui::VKEY_SPACE,
                                ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);

  EXPECT_FALSE(fake_listener.IsRegistered(default_hotkey));
  EXPECT_TRUE(fake_listener.IsRegistered(custom_hotkey));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_OnInvokeCommandLineShowsUI OnInvokeCommandLineShowsUI
#else
#define MAYBE_OnInvokeCommandLineShowsUI DISABLED_OnInvokeCommandLineShowsUI
#endif
TEST_F(OmniboxEverywhereControllerTest, MAYBE_OnInvokeCommandLineShowsUI) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kCommandLine,
                      profile_.get(), GetContext());
  EXPECT_EQ(profile_.get(), controller.target_profile());
  EXPECT_TRUE(controller.IsVisible());

  // Repeated kCommandLine invocation should keep the widget open (not
  // toggle-closed).
  controller.OnInvoke(omnibox_everywhere::InvocationSource::kCommandLine,
                      profile_.get(), GetContext());
  EXPECT_TRUE(controller.IsVisible());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InvokeForStartupWithLoadedProfile \
  InvokeForStartupWithLoadedProfile
#else
#define MAYBE_InvokeForStartupWithLoadedProfile \
  DISABLED_InvokeForStartupWithLoadedProfile
#endif
TEST_F(OmniboxEverywhereControllerTest,
       MAYBE_InvokeForStartupWithLoadedProfile) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Launching from command line with an eligible profile succeeds and shows UI.
  EXPECT_TRUE(controller.InvokeForStartup(
      omnibox_everywhere::InvocationSource::kCommandLine, profile_.get(),
      GetContext()));
  EXPECT_EQ(profile_.get(), controller.target_profile());
  EXPECT_TRUE(controller.IsVisible());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InvokeForStartupIneligibleFails InvokeForStartupIneligibleFails
#else
#define MAYBE_InvokeForStartupIneligibleFails \
  DISABLED_InvokeForStartupIneligibleFails
#endif
TEST_F(OmniboxEverywhereControllerTest, MAYBE_InvokeForStartupIneligibleFails) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Launching with null profile fails and does not show UI.
  EXPECT_FALSE(controller.InvokeForStartup(
      omnibox_everywhere::InvocationSource::kCommandLine, nullptr,
      GetContext()));
  EXPECT_FALSE(controller.IsVisible());

  // Launching with off-the-record profile fails.
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);
  EXPECT_FALSE(controller.InvokeForStartup(
      omnibox_everywhere::InvocationSource::kCommandLine, otr_profile,
      GetContext()));
  EXPECT_FALSE(controller.IsVisible());

  // Launching with non-Google DSE profile fails.
  TestingProfile non_dse_profile;
  EXPECT_FALSE(controller.InvokeForStartup(
      omnibox_everywhere::InvocationSource::kCommandLine, &non_dse_profile,
      GetContext()));
  EXPECT_FALSE(controller.IsVisible());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InvokeForStartupIgnoresInvalidPersistedPath \
  InvokeForStartupIgnoresInvalidPersistedPath
#else
#define MAYBE_InvokeForStartupIgnoresInvalidPersistedPath \
  DISABLED_InvokeForStartupIgnoresInvalidPersistedPath
#endif
TEST_F(OmniboxEverywhereControllerTest,
       MAYBE_InvokeForStartupIgnoresInvalidPersistedPath) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  // Set an invalid persisted profile path that does not exist in storage.
  local_state->SetFilePath(
      omnibox_everywhere::prefs::kLastTargetProfileDir,
      base::FilePath(FILE_PATH_LITERAL("non_existent_profile_path")));

  // InvokeForStartup should ignore invalid persisted path and fallback to
  // the provided eligible fallback_profile.
  EXPECT_TRUE(controller.InvokeForStartup(
      omnibox_everywhere::InvocationSource::kCommandLine, profile_.get(),
      GetContext()));
  EXPECT_EQ(profile_.get(), controller.target_profile());
  EXPECT_TRUE(controller.IsVisible());
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereControllerTest, CreateStartMenuShortcut) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir.GetPath());

  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  omnibox_everywhere::OmniboxEverywhereController controller;
  base::test::TestFuture<bool> future;
  controller.CreateStartMenuShortcut(future.GetCallback());
  EXPECT_TRUE(future.Get());
}
#endif

TEST_F(OmniboxEverywhereControllerTest, PinToTaskbar) {
  omnibox_everywhere::OmniboxEverywhereController controller;
  base::test::TestFuture<bool> future;
  controller.OfferPinToTaskbar(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(OmniboxEverywhereControllerTest,
       DisabledOmniboxEverywhereBlocksHotkeyRegistration) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();

  FakeGlobalAcceleratorListener fake_listener;
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }),
      &fake_listener);

  ui::Accelerator default_hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  EXPECT_TRUE(fake_listener.IsRegistered(default_hotkey));

  local_state->SetBoolean(omnibox_everywhere::prefs::kOmniboxEverywhereEnabled,
                          false);
  EXPECT_FALSE(fake_listener.IsRegistered(default_hotkey));

  local_state->SetBoolean(omnibox_everywhere::prefs::kOmniboxEverywhereEnabled,
                          true);
  EXPECT_TRUE(fake_listener.IsRegistered(default_hotkey));
}

TEST_F(OmniboxEverywhereControllerTest,
       DisabledOmniboxEverywhereBlocksInvocation) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetBoolean(omnibox_everywhere::prefs::kOmniboxEverywhereEnabled,
                          false);

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kStatusTrayIcon,
                      profile_.get(), GetContext());
  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kProfilePicker,
                      profile_.get());
  EXPECT_FALSE(controller.IsVisible());
}

TEST_F(OmniboxEverywhereControllerTest, DisabledHotkeyBlocksHotkeyInvocation) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetBoolean(omnibox_everywhere::prefs::kOmniboxEverywhereEnabled,
                          true);
  local_state->SetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled, false);

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      profile_.get(), GetContext());
  EXPECT_FALSE(controller.IsVisible());
}
