// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include <set>

#include "base/test/scoped_feature_list.h"
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
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  // SetTargetProfile with an off-the-record profile resolves to its original
  // profile.
  controller.SetTargetProfile(otr_profile);
  EXPECT_EQ(&profile_, controller.target_profile());

  // Activating an off-the-record browser window also resolves to its original
  // profile.
  MockBrowserWindowInterface otr_bwi;
  EXPECT_CALL(otr_bwi, GetProfile())
      .WillRepeatedly(testing::Return(otr_profile));

  controller.OnBrowserActivated(&otr_bwi);
  EXPECT_EQ(&profile_, controller.target_profile());
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
  controller.SetTargetProfile(&profile_);
  EXPECT_EQ(local_state->GetFilePath(
                omnibox_everywhere::prefs::kLastTargetProfileDir),
            profile_.GetPath());
}

TEST_F(OmniboxEverywhereControllerTest, RestoresTargetProfileOnStartup) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetFilePath(omnibox_everywhere::prefs::kLastTargetProfileDir,
                           profile_.GetPath());

  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Simulating OnProfileAdded with matching profile path should restore it as
  // target_profile().
  controller.OnProfileAdded(&profile_);
  EXPECT_EQ(&profile_, controller.target_profile());
  EXPECT_EQ(local_state->GetFilePath(
                omnibox_everywhere::prefs::kLastTargetProfileDir),
            profile_.GetPath());
}

TEST_F(OmniboxEverywhereControllerTest,
       RestoresMatchingPersistedProfileWhenMultipleProfilesAdded) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  TestingProfile profile1;

  // Persist profile_'s path as the target profile.
  local_state->SetFilePath(omnibox_everywhere::prefs::kLastTargetProfileDir,
                           profile_.GetPath());

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
  controller.OnProfileAdded(&profile_);
  EXPECT_EQ(&profile_, controller.target_profile());
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
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  // OnProfileAdded ignores off-the-record profile.
  controller.OnProfileAdded(otr_profile);
  EXPECT_EQ(nullptr, controller.target_profile());
  EXPECT_TRUE(
      local_state->GetFilePath(omnibox_everywhere::prefs::kLastTargetProfileDir)
          .empty());

  // Normal regular profile is accepted when added.
  controller.OnProfileAdded(&profile_);
  EXPECT_EQ(&profile_, controller.target_profile());
}

TEST_F(OmniboxEverywhereControllerTest,
       IneligibleProfileInSetTargetProfileIsNoOp) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  // Initially set an eligible target profile.
  controller.SetTargetProfile(&profile_);
  EXPECT_EQ(&profile_, controller.target_profile());

  // Passing an ineligible profile (e.g. missing service) is a NO-OP.
  TestingProfile ineligible_profile;
  controller.SetTargetProfile(&ineligible_profile);
  EXPECT_EQ(&profile_, controller.target_profile());

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
  EXPECT_CALL(bwi1, GetProfile()).WillRepeatedly(testing::Return(&profile_));

  MockBrowserWindowInterface bwi2;
  EXPECT_CALL(bwi2, GetProfile()).WillRepeatedly(testing::Return(&profile2));

  // Activating bwi1 sets target profile to profile_.
  controller.OnBrowserActivated(&bwi1);
  EXPECT_EQ(&profile_, controller.target_profile());

  // Activating bwi2 updates target profile to profile2.
  controller.OnBrowserActivated(&bwi2);
  EXPECT_EQ(&profile2, controller.target_profile());

  // Re-activating bwi1 updates target profile back to profile_.
  controller.OnBrowserActivated(&bwi1);
  EXPECT_EQ(&profile_, controller.target_profile());
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
                      &profile_, GetContext());
  EXPECT_EQ(&profile_, controller.target_profile());
  EXPECT_TRUE(controller.IsVisible());

  // Repeated kCommandLine invocation should keep the widget open (not
  // toggle-closed).
  controller.OnInvoke(omnibox_everywhere::InvocationSource::kCommandLine,
                      &profile_, GetContext());
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
      omnibox_everywhere::InvocationSource::kCommandLine, &profile_,
      GetContext()));
  EXPECT_EQ(&profile_, controller.target_profile());
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
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
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
      omnibox_everywhere::InvocationSource::kCommandLine, &profile_,
      GetContext()));
  EXPECT_EQ(&profile_, controller.target_profile());
  EXPECT_TRUE(controller.IsVisible());
}
