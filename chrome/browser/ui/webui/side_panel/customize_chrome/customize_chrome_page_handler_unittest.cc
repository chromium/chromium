// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/themes/ntp_background_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif

namespace content {
class BrowserContext;
}  // namespace content

namespace {

using testing::_;
using testing::An;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

// A test SelectFileDialog to go straight to calling the listener.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       const bool auto_cancel)
      : ui::SelectFileDialog(listener, std::move(policy)),
        auto_cancel_(auto_cancel) {}

  TestSelectFileDialog(const TestSelectFileDialog&) = delete;
  TestSelectFileDialog& operator=(const TestSelectFileDialog&) = delete;

 protected:
  ~TestSelectFileDialog() override = default;

  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    if (auto_cancel_) {
      listener_->FileSelectionCanceled();
    } else {
      base::FilePath path(FILE_PATH_LITERAL("/test/path"));
      listener_->FileSelected(ui::SelectedFileInfo(path), file_type_index);
    }
  }
  // Pure virtual methods that need to be implemented.
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  bool auto_cancel_;
};

// A test SelectFileDialogFactory so that the TestSelectFileDialog is used.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(bool auto_cancel)
      : auto_cancel_(auto_cancel) {}

  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, nullptr, auto_cancel_);
  }

 private:
  bool auto_cancel_;
};

class MockPage : public side_panel::mojom::CustomizeChromePage {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<side_panel::mojom::CustomizeChromePage>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD3(
      SetModulesSettings,
      void(std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings,
           bool managed,
           bool visible));
  MOCK_METHOD(void,
              SetMostVisitedSettings,
              (const std::vector<ntp_tiles::TileType>&,
               bool,
               bool,
               const std::vector<ntp_tiles::TileType>&));
  MOCK_METHOD(void, SetTheme, (side_panel::mojom::ThemePtr));
  MOCK_METHOD(void, SetThemeEditable, (bool));
  MOCK_METHOD(void, ScrollToSection, (CustomizeChromeSection));
  MOCK_METHOD(void,
              AttachedTabStateUpdated,
              (side_panel::mojom::NewTabPageType));
  MOCK_METHOD(void,
              NtpManagedByNameUpdated,
              (const std::string&, const std::string&));
  MOCK_METHOD(void, SetToolsSettings, (bool visible));
  MOCK_METHOD(
      void,
      SetFooterSettings,
      (bool visible,
       bool extension_policy_enabled,
       side_panel::mojom::ManagementNoticeStatePtr management_notice_state));

  mojo::Receiver<side_panel::mojom::CustomizeChromePage> receiver_{this};
};

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD(std::optional<CustomBackground>, GetCustomBackground, ());
  MOCK_METHOD(void, ResetCustomBackgroundInfo, ());
  MOCK_METHOD(void, SelectLocalBackgroundImage, (const base::FilePath&));
  MOCK_METHOD(void, AddObserver, (NtpCustomBackgroundServiceObserver*));
  MOCK_METHOD(void,
              SetCustomBackgroundInfo,
              (const GURL&,
               const GURL&,
               const std::string&,
               const std::string&,
               const GURL&,
               const std::string&));
  MOCK_METHOD(bool, IsCustomBackgroundDisabledByPolicy, ());
};

class MockNtpBackgroundService : public NtpBackgroundService {
 public:
  explicit MockNtpBackgroundService(
      ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : NtpBackgroundService(application_locale_storage, url_loader_factory) {}
  MOCK_CONST_METHOD0(collection_info, std::vector<CollectionInfo>&());
  MOCK_CONST_METHOD0(collection_images, std::vector<CollectionImage>&());
  MOCK_METHOD(void, FetchCollectionInfo, (const std::string& filtering_label));
  MOCK_METHOD(void, FetchCollectionInfo, ());
  MOCK_METHOD(void, FetchCollectionImageInfo, (const std::string&));
  MOCK_METHOD(void,
              FetchReplacementCollectionPreviewImage,
              (const std::string&,
               NtpBackgroundService::FetchReplacementImageCallback));
  MOCK_METHOD(void, AddObserver, (NtpBackgroundServiceObserver*));
};

class MockThemeService : public ThemeService {
 public:
  MockThemeService() : ThemeService(nullptr, theme_helper_) { set_ready(); }
  using ThemeService::NotifyThemeChanged;
  MOCK_METHOD(void, UseDefaultTheme, ());
  MOCK_METHOD(void, UseDeviceTheme, (bool));
  MOCK_CONST_METHOD0(UsingDefaultTheme, bool());
  MOCK_CONST_METHOD0(UsingSystemTheme, bool());
  MOCK_CONST_METHOD0(UsingExtensionTheme, bool());
  MOCK_CONST_METHOD0(GetThemeID, std::string());
  MOCK_CONST_METHOD0(GetUserColor, std::optional<SkColor>());
  MOCK_CONST_METHOD0(UsingDeviceTheme, bool());

 private:
  ThemeHelper theme_helper_;
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      NtpBackgroundServiceFactory::GetInstance(),
      base::BindRepeating(
          [](ApplicationLocaleStorage* application_locale_storage,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<MockNtpBackgroundService>>(
                application_locale_storage, url_loader_factory);
          },
          application_locale_storage, url_loader_factory));
  profile_builder.AddTestingFactory(
      ThemeServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockThemeService>>();
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  return profile;
}

}  // namespace

class CustomizeChromePageHandlerTest : public testing::Test {
 public:
  CustomizeChromePageHandlerTest()
      : application_locale_storage_(TestingBrowserProcess::GetGlobal()
                                        ->GetFeatures()
                                        ->application_locale_storage()),
        profile_(
            MakeTestingProfile(application_locale_storage_.get(),
                               test_url_loader_factory_.GetSafeWeakWrapper())),
        mock_ntp_custom_background_service_(profile_.get()),
        mock_ntp_background_service_(static_cast<MockNtpBackgroundService*>(
            NtpBackgroundServiceFactory::GetForProfile(profile_.get()))),
        web_contents_(web_contents_factory_.CreateWebContents(profile_.get())),
        mock_theme_service_(static_cast<MockThemeService*>(
            ThemeServiceFactory::GetForProfile(profile_.get()))) {}

  void SetUp() override {
    EXPECT_CALL(mock_ntp_background_service(), AddObserver)
        .Times(1)
        .WillOnce(SaveArg<0>(&ntp_background_service_observer_));
    EXPECT_CALL(mock_ntp_custom_background_service_, AddObserver)
        .Times(1)
        .WillOnce(SaveArg<0>(&ntp_custom_background_service_observer_));
    const std::vector<ntp::ModuleIdDetail> module_id_details = {
        {ntp_modules::kMostRelevantTabResumptionModuleId,
         IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE},
        {ntp_modules::kMicrosoftAuthenticationModuleId,
         IDS_NTP_MODULES_MICROSOFT_AUTHENTICATION_NAME,
         IDS_NTP_MICROSOFT_AUTHENTICATION_SIDE_PANEL_DESCRIPTION}};
    handler_ = std::make_unique<CustomizeChromePageHandler>(
        mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>(),
        mock_page_.BindAndGetRemote(), &mock_ntp_custom_background_service_,
        web_contents_, module_id_details, mock_open_url_callback_.Get());
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), ntp_background_service_observer_);
    EXPECT_EQ(handler_.get(), ntp_custom_background_service_observer_);

    auto browser_window = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams browser_params(profile_.get(), true);
    browser_params.type = Browser::TYPE_NORMAL;
    browser_params.window = browser_window.release();
    browser_ = Browser::DeprecatedCreateOwnedForTesting(browser_params);

    application_locale_storage_->Set("foo");

    scoped_feature_list_.Reset();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    browser_->tab_strip_model()->CloseAllTabs();
    browser_.reset();
    test_url_loader_factory_.ClearResponses();
  }

  void SetMostVisitedPrefs(bool custom_links_visible,
                           bool enterprise_shortcuts_visible,
                           bool shortcuts_visible,
                           bool personal_shortcuts_visible) {
    profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible,
                                     custom_links_visible);
    profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                     enterprise_shortcuts_visible);
    profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible,
                                     shortcuts_visible);
    profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                     personal_shortcuts_visible);
  }

  void CheckMostVisitedPrefs(bool custom_links_visible,
                             bool enterprise_shortcuts_visible,
                             bool shortcuts_visible,
                             bool personal_shortcuts_visible) {
    EXPECT_EQ(custom_links_visible, profile().GetPrefs()->GetBoolean(
                                        ntp_prefs::kNtpCustomLinksVisible));
    EXPECT_EQ(enterprise_shortcuts_visible,
              profile().GetPrefs()->GetBoolean(
                  ntp_prefs::kNtpEnterpriseShortcutsVisible));
    EXPECT_EQ(shortcuts_visible, profile().GetPrefs()->GetBoolean(
                                     ntp_prefs::kNtpShortcutsVisible));
    EXPECT_EQ(personal_shortcuts_visible,
              profile().GetPrefs()->GetBoolean(
                  ntp_prefs::kNtpPersonalShortcutsVisible));
  }

  void CheckHistograms(const std::string& name, const auto& counts) {
    int total = 0;
    for (const auto& [action, count] : counts) {
      histogram_tester().ExpectBucketCount(name, action, count);
      total += count;
    }
    histogram_tester().ExpectTotalCount(name, total);
  }

  void SetEnterpriseShortcutsPolicy(bool has_policy) {
    if (has_policy) {
      base::Value::List enterprise_shortcuts;
      enterprise_shortcuts.Append(base::Value::Dict()
                                      .Set("title", "test")
                                      .Set("url", "https://test.com"));
      profile().GetPrefs()->SetList(
          ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
          std::move(enterprise_shortcuts));
    } else {
      profile().GetPrefs()->SetList(
          ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
          base::Value::List());
    }
  }

  TestingProfile& profile() { return *profile_; }
  content::WebContents& web_contents() { return *web_contents_; }
  CustomizeChromePageHandler& handler() { return *handler_; }
  NtpCustomBackgroundServiceObserver& ntp_custom_background_service_observer() {
    return *ntp_custom_background_service_observer_;
  }
  MockNtpBackgroundService& mock_ntp_background_service() {
    return *mock_ntp_background_service_;
  }
  NtpBackgroundServiceObserver& ntp_background_service_observer() {
    return *ntp_background_service_observer_;
  }
  MockThemeService& mock_theme_service() { return *mock_theme_service_; }
  Browser& browser() { return *browser_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  base::UserActionTester& user_action_tester() { return user_action_tester_; }

 protected:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<ApplicationLocaleStorage> application_locale_storage_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNtpCustomBackgroundService>
      mock_ntp_custom_background_service_;
  raw_ptr<MockNtpBackgroundService> mock_ntp_background_service_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockPage> mock_page_;
  raw_ptr<MockThemeService> mock_theme_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<Browser> browser_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::MockRepeatingCallback<void(const GURL& gurl)> mock_open_url_callback_;
  std::unique_ptr<CustomizeChromePageHandler> handler_;
  raw_ptr<NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observer_;
  raw_ptr<NtpBackgroundServiceObserver> ntp_background_service_observer_;
};

struct SetMostVisitedSettingsTestCase {
  std::string test_name;
  // Initial state
  bool initial_custom_links_visible;
  bool initial_enterprise_shortcuts_visible;
  bool initial_shortcuts_visible;
  bool initial_personal_shortcuts_visible;
  bool has_enterprise_policy;
  // Action
  std::vector<ntp_tiles::TileType> types_to_set;
  bool visible_to_set;
  bool personal_shortcuts_visible_to_set;
  // Expected final state
  bool expected_custom_links_visible;
  bool expected_enterprise_shortcuts_visible;
  bool expected_shortcuts_visible;
  bool expected_personal_shortcuts_visible;
  std::map<CustomizeShortcutAction, int> expected_histogram_counts;
};

class CustomizeChromePageHandlerSetMostVisitedTest
    : public CustomizeChromePageHandlerTest,
      public ::testing::WithParamInterface<SetMostVisitedSettingsTestCase> {};

TEST_P(CustomizeChromePageHandlerSetMostVisitedTest, SetMostVisitedSettings) {
  const auto& test_case = GetParam();

  std::vector<ntp_tiles::TileType> types;
  bool visible;
  bool personal_shortcuts_visible;
  std::vector<ntp_tiles::TileType> disabled_shortcuts;
  EXPECT_CALL(mock_page_, SetMostVisitedSettings)
      .WillRepeatedly(DoAll(SaveArg<0>(&types), SaveArg<1>(&visible),
                            SaveArg<2>(&personal_shortcuts_visible),
                            SaveArg<3>(&disabled_shortcuts)));

  // Set initial enterprise shortcuts policy and prefs state.
  SetEnterpriseShortcutsPolicy(test_case.has_enterprise_policy);
  SetMostVisitedPrefs(test_case.initial_custom_links_visible,
                      test_case.initial_enterprise_shortcuts_visible,
                      test_case.initial_shortcuts_visible,
                      test_case.initial_personal_shortcuts_visible);

  // Validate initial prefs state.
  histogram_tester().ExpectTotalCount("NewTabPage.CustomizeShortcutAction", 0);
  CheckMostVisitedPrefs(test_case.initial_custom_links_visible,
                        test_case.initial_enterprise_shortcuts_visible,
                        test_case.initial_shortcuts_visible,
                        test_case.initial_personal_shortcuts_visible);

  // Call SetMostVisitedSettings handler.
  handler().SetMostVisitedSettings(test_case.types_to_set,
                                   test_case.visible_to_set,
                                   test_case.personal_shortcuts_visible_to_set);
  mock_page_.FlushForTesting();

  // Validate updated prefs state.
  CheckMostVisitedPrefs(test_case.expected_custom_links_visible,
                        test_case.expected_enterprise_shortcuts_visible,
                        test_case.expected_shortcuts_visible,
                        test_case.expected_personal_shortcuts_visible);

  // Validate histograms.
  CheckHistograms("NewTabPage.CustomizeShortcutAction",
                  test_case.expected_histogram_counts);
}

const SetMostVisitedSettingsTestCase kSetMostVisitedSettingsTestCases[] = {
    {.test_name = "SetSingleType",
     .initial_custom_links_visible = true,
     .initial_enterprise_shortcuts_visible = true,
     .initial_shortcuts_visible = true,
     .initial_personal_shortcuts_visible = true,
     .has_enterprise_policy = true,
     .types_to_set = {ntp_tiles::TileType::kTopSites},
     .visible_to_set = true,
     .personal_shortcuts_visible_to_set = true,
     .expected_custom_links_visible = false,
     .expected_enterprise_shortcuts_visible = false,
     .expected_shortcuts_visible = true,
     .expected_personal_shortcuts_visible = true,
     .expected_histogram_counts =
         {{CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_TYPE, 1},
          {CustomizeShortcutAction::
               CUSTOMIZE_ENTERPRISE_SHORTCUT_ACTION_TOGGLE_VISIBILITY,
           1}}},
    {.test_name = "SetSingleType_EnterprisePolicyEmpty",
     .initial_custom_links_visible = true,
     .initial_enterprise_shortcuts_visible = true,
     .initial_shortcuts_visible = true,
     .initial_personal_shortcuts_visible = true,
     .has_enterprise_policy = false,
     .types_to_set = {ntp_tiles::TileType::kTopSites},
     .visible_to_set = true,
     .personal_shortcuts_visible_to_set = true,
     .expected_custom_links_visible = false,
     .expected_enterprise_shortcuts_visible = true,
     .expected_shortcuts_visible = true,
     .expected_personal_shortcuts_visible = true,
     .expected_histogram_counts =
         {{CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_TYPE, 1}}},
    {.test_name = "SetMultipleTypes",
     .initial_custom_links_visible = true,
     .initial_enterprise_shortcuts_visible = false,
     .initial_shortcuts_visible = true,
     .initial_personal_shortcuts_visible = true,
     .has_enterprise_policy = true,
     .types_to_set = {ntp_tiles::TileType::kTopSites,
                      ntp_tiles::TileType::kEnterpriseShortcuts},
     .visible_to_set = true,
     .personal_shortcuts_visible_to_set = true,
     .expected_custom_links_visible = false,
     .expected_enterprise_shortcuts_visible = true,
     .expected_shortcuts_visible = true,
     .expected_personal_shortcuts_visible = true,
     .expected_histogram_counts =
         {{CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_TYPE, 1},
          {CustomizeShortcutAction::
               CUSTOMIZE_ENTERPRISE_SHORTCUT_ACTION_TOGGLE_VISIBILITY,
           1}}},
    {.test_name = "SetMultipleTypes_EnterprisePolicyEmpty",
     .initial_custom_links_visible = true,
     .initial_enterprise_shortcuts_visible = false,
     .initial_shortcuts_visible = true,
     .initial_personal_shortcuts_visible = true,
     .has_enterprise_policy = false,
     .types_to_set = {ntp_tiles::TileType::kTopSites,
                      ntp_tiles::TileType::kEnterpriseShortcuts},
     .visible_to_set = true,
     .personal_shortcuts_visible_to_set = true,
     .expected_custom_links_visible = false,
     .expected_enterprise_shortcuts_visible = false,
     .expected_shortcuts_visible = true,
     .expected_personal_shortcuts_visible = true,
     .expected_histogram_counts =
         {{CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_TYPE, 1}}},
    {.test_name = "SetShortcutsVisible",
     .initial_custom_links_visible = true,
     .initial_enterprise_shortcuts_visible = false,
     .initial_shortcuts_visible = false,
     .initial_personal_shortcuts_visible = true,
     .has_enterprise_policy = true,
     .types_to_set = {ntp_tiles::TileType::kCustomLinks},
     .visible_to_set = true,
     .personal_shortcuts_visible_to_set = true,
     .expected_custom_links_visible = true,
     .expected_enterprise_shortcuts_visible = false,
     .expected_shortcuts_visible = true,
     .expected_personal_shortcuts_visible = true,
     .expected_histogram_counts =
         {{CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_VISIBILITY,
           1}}},
    {.test_name = "SetPersonalShortcutsVisible",
     .initial_custom_links_visible = true,
     .initial_enterprise_shortcuts_visible = false,
     .initial_shortcuts_visible = true,
     .initial_personal_shortcuts_visible = true,
     .has_enterprise_policy = true,
     .types_to_set = {ntp_tiles::TileType::kCustomLinks},
     .visible_to_set = true,
     .personal_shortcuts_visible_to_set = false,
     .expected_custom_links_visible = true,
     .expected_enterprise_shortcuts_visible = false,
     .expected_shortcuts_visible = true,
     .expected_personal_shortcuts_visible = false,
     .expected_histogram_counts = {
         {CustomizeShortcutAction::
              CUSTOMIZE_PERSONAL_SHORTCUT_ACTION_TOGGLE_VISIBILITY,
          1}}}};

INSTANTIATE_TEST_SUITE_P(
    All,
    CustomizeChromePageHandlerSetMostVisitedTest,
    ::testing::ValuesIn(kSetMostVisitedSettingsTestCases),
    [](const testing::TestParamInfo<SetMostVisitedSettingsTestCase>& info) {
      return info.param.test_name;
    });

struct UpdateMostVisitedSettingsTestCase {
  std::string test_name;
  // Initial state
  bool enterprise_shortcuts_feature_enabled;
  bool enterprise_shortcuts_mixing_enabled;
  bool has_enterprise_policy;
  bool custom_links_visible;
  bool enterprise_shortcuts_visible;
  bool personal_shortcuts_visible;
  // Expected state sent to UI
  std::vector<ntp_tiles::TileType> expected_types;
  std::vector<ntp_tiles::TileType> expected_disabled_shortcuts;
};

class CustomizeChromePageHandlerUpdateMostVisitedTest
    : public CustomizeChromePageHandlerTest,
      public ::testing::WithParamInterface<UpdateMostVisitedSettingsTestCase> {
};

TEST_P(CustomizeChromePageHandlerUpdateMostVisitedTest,
       UpdateMostVisitedSettings) {
  const auto& test_case = GetParam();

  base::test::ScopedFeatureList features;
  if (test_case.enterprise_shortcuts_feature_enabled) {
    if (test_case.enterprise_shortcuts_mixing_enabled) {
      features.InitAndEnableFeatureWithParameters(
          ntp_tiles::kNtpEnterpriseShortcuts,
          {{ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.name, "true"}});
    } else {
      features.InitAndEnableFeatureWithParameters(
          ntp_tiles::kNtpEnterpriseShortcuts,
          {{ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.name, "false"}});
    }
  } else {
    features.InitAndDisableFeature(ntp_tiles::kNtpEnterpriseShortcuts);
  }

  std::vector<ntp_tiles::TileType> types;
  bool visible;
  bool personal_shortcuts_visible;
  std::vector<ntp_tiles::TileType> disabled_shortcuts;
  EXPECT_CALL(mock_page_, SetMostVisitedSettings)
      .WillRepeatedly(DoAll(SaveArg<0>(&types), SaveArg<1>(&visible),
                            SaveArg<2>(&personal_shortcuts_visible),
                            SaveArg<3>(&disabled_shortcuts)));

  // Set initial enterprise shortcuts policy and prefs state.
  SetEnterpriseShortcutsPolicy(test_case.has_enterprise_policy);
  SetMostVisitedPrefs(
      test_case.custom_links_visible, test_case.enterprise_shortcuts_visible,
      /*shortcuts_visible=*/true, test_case.personal_shortcuts_visible);
  mock_page_.FlushForTesting();

  // Validate returned types and disbaled_shortcuts from handler.
  EXPECT_THAT(types,
              testing::UnorderedElementsAreArray(test_case.expected_types));
  EXPECT_THAT(disabled_shortcuts, testing::UnorderedElementsAreArray(
                                      test_case.expected_disabled_shortcuts));
}

const UpdateMostVisitedSettingsTestCase kUpdateMostVisitedSettingsTestCases[] =
    {{.test_name = "EnterpriseFeatureDisabled_PersonalShortcutsVisible",
      .enterprise_shortcuts_feature_enabled = false,
      .enterprise_shortcuts_mixing_enabled = false,
      .has_enterprise_policy = true,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = true,
      .expected_types = {ntp_tiles::TileType::kCustomLinks,
                         ntp_tiles::TileType::kEnterpriseShortcuts},
      .expected_disabled_shortcuts =
          {ntp_tiles::TileType::kEnterpriseShortcuts}},
     {.test_name = "EnterpriseFeatureDisabled_PersonalShortcutsNotVisible",
      .enterprise_shortcuts_feature_enabled = false,
      .enterprise_shortcuts_mixing_enabled = false,
      .has_enterprise_policy = true,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = false,
      .expected_types = {ntp_tiles::TileType::kCustomLinks,
                         ntp_tiles::TileType::kEnterpriseShortcuts},
      .expected_disabled_shortcuts =
          {ntp_tiles::TileType::kEnterpriseShortcuts}},
     {.test_name = "EnterpriseMixingFeatureDisabled_EnterprisePolicyEmpty",
      .enterprise_shortcuts_feature_enabled = true,
      .enterprise_shortcuts_mixing_enabled = false,
      .has_enterprise_policy = false,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = true,
      .expected_types = {ntp_tiles::TileType::kEnterpriseShortcuts},
      .expected_disabled_shortcuts =
          {ntp_tiles::TileType::kEnterpriseShortcuts}},
     {.test_name = "EnterpriseMixingFeatureDisabled_PersonalShortcutsVisible",
      .enterprise_shortcuts_feature_enabled = true,
      .enterprise_shortcuts_mixing_enabled = false,
      .has_enterprise_policy = true,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = true,
      .expected_types = {ntp_tiles::TileType::kEnterpriseShortcuts},
      .expected_disabled_shortcuts = {}},
     {.test_name =
          "EnterpriseMixingFeatureDisabled_PersonalShortcutsNotVisible",
      .enterprise_shortcuts_feature_enabled = true,
      .enterprise_shortcuts_mixing_enabled = false,
      .has_enterprise_policy = true,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = false,
      .expected_types = {ntp_tiles::TileType::kEnterpriseShortcuts},
      .expected_disabled_shortcuts = {}},
     {.test_name = "EnterpriseMixingFeatureEnabled_EnteprisePolicyEmpty",
      .enterprise_shortcuts_feature_enabled = true,
      .enterprise_shortcuts_mixing_enabled = true,
      .has_enterprise_policy = false,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = true,
      .expected_types = {ntp_tiles::TileType::kEnterpriseShortcuts,
                         ntp_tiles::TileType::kCustomLinks},
      .expected_disabled_shortcuts =
          {ntp_tiles::TileType::kEnterpriseShortcuts}},
     {.test_name = "EnterpriseMixingFeatureEnabled_PersonalShortcutsVisible",
      .enterprise_shortcuts_feature_enabled = true,
      .enterprise_shortcuts_mixing_enabled = true,
      .has_enterprise_policy = true,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = true,
      .expected_types = {ntp_tiles::TileType::kEnterpriseShortcuts,
                         ntp_tiles::TileType::kCustomLinks},
      .expected_disabled_shortcuts = {}},
     {.test_name = "EnterpriseMixingFeatureEnabled_PersonalShortcutsNotVisible",
      .enterprise_shortcuts_feature_enabled = true,
      .enterprise_shortcuts_mixing_enabled = true,
      .has_enterprise_policy = true,
      .custom_links_visible = true,
      .enterprise_shortcuts_visible = true,
      .personal_shortcuts_visible = false,
      .expected_types = {ntp_tiles::TileType::kEnterpriseShortcuts},
      .expected_disabled_shortcuts = {}}};

INSTANTIATE_TEST_SUITE_P(
    All,
    CustomizeChromePageHandlerUpdateMostVisitedTest,
    ::testing::ValuesIn(kUpdateMostVisitedSettingsTestCases),
    [](const testing::TestParamInfo<UpdateMostVisitedSettingsTestCase>& info) {
      return info.param.test_name;
    });

TEST_F(CustomizeChromePageHandlerTest,
       UpdateMostVisitedSettingsOnPolicyChange) {
  std::vector<ntp_tiles::TileType> types;
  bool visible;
  bool personal_shortcuts_visible;
  std::vector<ntp_tiles::TileType> disabled_shortcuts;
  EXPECT_CALL(mock_page_, SetMostVisitedSettings)
      .WillRepeatedly(DoAll(SaveArg<0>(&types), SaveArg<1>(&visible),
                            SaveArg<2>(&personal_shortcuts_visible),
                            SaveArg<3>(&disabled_shortcuts)));

  // Enable enterprise shortcuts policy with mixing disabled.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      ntp_tiles::kNtpEnterpriseShortcuts,
      {{ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.name, "false"}});

  SetEnterpriseShortcutsPolicy(true);
  SetMostVisitedPrefs(
      /*custom_links_visible=*/true, /*enterprise_shortcuts_visible=*/true,
      /*shortcuts_visible=*/true, /*personal_shortcuts_visible=*/true);
  mock_page_.FlushForTesting();

  // The enterprise shortcuts option should be visible.
  EXPECT_EQ(0u, disabled_shortcuts.size());

  // Set shortcut type to enterprise.
  handler().SetMostVisitedSettings(
      /*types=*/{ntp_tiles::TileType::kEnterpriseShortcuts}, /*visible=*/true,
      /*personal_shortcuts_visible=*/true);
  mock_page_.FlushForTesting();

  // Verify state.
  EXPECT_THAT(types, testing::UnorderedElementsAre(
                         ntp_tiles::TileType::kEnterpriseShortcuts));
  EXPECT_TRUE(personal_shortcuts_visible);
  EXPECT_EQ(0u, disabled_shortcuts.size());

  // Set enterprise shortcuts policy to empty list.
  SetEnterpriseShortcutsPolicy(false);
  mock_page_.FlushForTesting();

  // Verify state is updated. The type should still contain enterprise shortcuts
  // since the pref hasn't been updated yet, but it should be disabled.
  // Personal shortcuts should become visible.
  EXPECT_THAT(types, testing::UnorderedElementsAre(
                         ntp_tiles::TileType::kEnterpriseShortcuts));
  EXPECT_TRUE(visible);
  EXPECT_TRUE(personal_shortcuts_visible);
  EXPECT_THAT(
      disabled_shortcuts,
      testing::UnorderedElementsAre(ntp_tiles::TileType::kEnterpriseShortcuts));
}

TEST_F(CustomizeChromePageHandlerTest,
       UpdateMostVisitedSettingsOnPolicyChange_MixingEnabled) {
  std::vector<ntp_tiles::TileType> types;
  bool visible;
  bool personal_shortcuts_visible;
  std::vector<ntp_tiles::TileType> disabled_shortcuts;
  EXPECT_CALL(mock_page_, SetMostVisitedSettings)
      .WillRepeatedly(DoAll(SaveArg<0>(&types), SaveArg<1>(&visible),
                            SaveArg<2>(&personal_shortcuts_visible),
                            SaveArg<3>(&disabled_shortcuts)));

  // Enable enterprise shortcuts policy.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      ntp_tiles::kNtpEnterpriseShortcuts,
      {{ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.name, "true"}});

  SetEnterpriseShortcutsPolicy(true);
  SetMostVisitedPrefs(
      /*custom_links_visible=*/true, /*enterprise_shortcuts_visible=*/true,
      /*shortcuts_visible=*/true, /*personal_shortcuts_visible=*/false);
  mock_page_.FlushForTesting();

  // The enterprise shortcuts option should be visible.
  EXPECT_EQ(0u, disabled_shortcuts.size());

  // Set shortcut type to enterprise (no personal shortcuts should be set since
  // they are not visible).
  handler().SetMostVisitedSettings(
      /*types=*/{ntp_tiles::TileType::kEnterpriseShortcuts}, /*visible=*/true,
      /*personal_shortcuts_visible=*/false);
  mock_page_.FlushForTesting();

  // Verify state.
  EXPECT_THAT(types, testing::UnorderedElementsAre(
                         ntp_tiles::TileType::kEnterpriseShortcuts));
  EXPECT_FALSE(personal_shortcuts_visible);
  EXPECT_EQ(0u, disabled_shortcuts.size());

  // Set enterprise shortcuts policy to empty list.
  SetEnterpriseShortcutsPolicy(false);
  mock_page_.FlushForTesting();

  // Verify state is updated. The type should fallback to the existing custom
  // links visibility pref and enterprise shortcuts should be disabled. Personal
  // shortcuts should become visible.
  EXPECT_THAT(types, testing::UnorderedElementsAre(
                         ntp_tiles::TileType::kEnterpriseShortcuts,
                         ntp_tiles::TileType::kCustomLinks));
  EXPECT_TRUE(visible);
  EXPECT_TRUE(personal_shortcuts_visible);
  EXPECT_THAT(
      disabled_shortcuts,
      testing::UnorderedElementsAre(ntp_tiles::TileType::kEnterpriseShortcuts));
}

enum class ThemeUpdateSource {
  kMojo,
  kThemeService,
  kNativeTheme,
  kCustomBackgroundService,
};

class CustomizeChromePageHandlerSetThemeTest
    : public CustomizeChromePageHandlerTest,
      public ::testing::WithParamInterface<ThemeUpdateSource> {
 public:
  side_panel::mojom::ThemePtr UpdateTheme() {
    // Flush any existing updates so the flush below only sees what results from
    // the update below.
    mock_page_.FlushForTesting();

    side_panel::mojom::ThemePtr theme;
    EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg(&theme));

    // Update.
    switch (GetParam()) {
      case ThemeUpdateSource::kMojo:
        handler().UpdateTheme();
        break;
      case ThemeUpdateSource::kThemeService:
        mock_theme_service().NotifyThemeChanged();
        break;
      case ThemeUpdateSource::kNativeTheme:
        ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
        break;
      case ThemeUpdateSource::kCustomBackgroundService:
        ntp_custom_background_service_observer()
            .OnCustomBackgroundImageUpdated();
        break;
    }
    mock_page_.FlushForTesting();

    // Should have seen exactly one attempt to set the theme.
    Mock::VerifyAndClearExpectations(&mock_page_);
    return theme;
  }
};

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetTheme) {
  ui::MockOsSettingsProvider os_settings_provider;
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.is_uploaded_image = false;
  custom_background.collection_id = "test_collection";
  custom_background.daily_refresh_enabled = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service(), GetUserColor())
      .WillByDefault(Return(std::optional<SkColor>()));
  ON_CALL(mock_theme_service(), UsingDefaultTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingSystemTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingDeviceTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_ntp_custom_background_service_,
          IsCustomBackgroundDisabledByPolicy())
      .WillByDefault(Return(true));
  os_settings_provider.SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  EXPECT_FALSE(theme->background_image->is_uploaded_image);
  EXPECT_FALSE(theme->background_image->daily_refresh_enabled);
  EXPECT_EQ("foo line", theme->background_image->title);
  EXPECT_EQ("test_collection", theme->background_image->collection_id);
  EXPECT_EQ(
      web_contents().GetColorProvider().GetColor(kColorNewTabPageBackground),
      theme->background_color);
  EXPECT_EQ(web_contents().GetColorProvider().GetColor(ui::kColorFrameActive),
            theme->foreground_color);
  EXPECT_TRUE(theme->background_managed_by_policy);
  EXPECT_FALSE(theme->follow_device_theme);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetThemeWithDailyRefresh) {
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.daily_refresh_enabled = true;
  custom_background.collection_id = "test_collection";
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_TRUE(theme->background_image->daily_refresh_enabled);
  EXPECT_EQ("test_collection", theme->background_image->collection_id);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetUploadedImage) {
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.is_uploaded_image = true;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service(), UsingDefaultTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingSystemTheme())
      .WillByDefault(Return(false));

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  ASSERT_TRUE(theme->background_image->is_uploaded_image);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetWallpaperSearchImage) {
  CustomBackground custom_background;
  base::Token token = base::Token::CreateRandom();
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.is_uploaded_image = true;
  custom_background.local_background_id = token;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service(), UsingDefaultTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingSystemTheme())
      .WillByDefault(Return(false));

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_TRUE(theme->background_image->is_uploaded_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  EXPECT_EQ(token, theme->background_image->local_background_id);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetThirdPartyTheme) {
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");

  auto* extension_registry = extensions::ExtensionRegistry::Get(profile_.get());
  scoped_refptr<const extensions::Extension> extension;
  extension = extensions::ExtensionBuilder()
                  .SetManifest(base::Value::Dict()
                                   .Set("name", "Foo Extension")
                                   .Set("version", "1.0.0")
                                   .Set("manifest_version", 2))
                  .SetID("foo")
                  .Build();
  extension_registry->AddEnabled(extension);

  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service(), UsingDefaultTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingExtensionTheme())
      .WillByDefault(Return(true));
  ON_CALL(mock_theme_service(), UsingSystemTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), GetThemeID()).WillByDefault(Return("foo"));

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  ASSERT_TRUE(theme->third_party_theme_info);
  EXPECT_EQ("foo", theme->third_party_theme_info->id);
  EXPECT_EQ("Foo Extension", theme->third_party_theme_info->name);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CustomizeChromePageHandlerSetThemeTest,
    ::testing::Values(ThemeUpdateSource::kMojo,
                      ThemeUpdateSource::kThemeService,
                      ThemeUpdateSource::kNativeTheme,
                      ThemeUpdateSource::kCustomBackgroundService));

TEST_F(CustomizeChromePageHandlerTest, GetBackgroundCollections) {
  std::vector<CollectionInfo> test_collection_info;
  CollectionInfo test_collection;
  test_collection.collection_id = "test_id";
  test_collection.collection_name = "test_name";
  test_collection.preview_image_url = GURL("https://test.jpg");
  test_collection_info.push_back(test_collection);
  ON_CALL(mock_ntp_background_service(), collection_info())
      .WillByDefault(ReturnRef(test_collection_info));

  std::vector<side_panel::mojom::BackgroundCollectionPtr> collections;
  base::MockCallback<
      CustomizeChromePageHandler::GetBackgroundCollectionsCallback>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&collections));
  EXPECT_CALL(mock_ntp_background_service(), FetchCollectionInfo()).Times(1);
  handler().GetBackgroundCollections(callback.Get());
  ntp_background_service_observer().OnCollectionInfoAvailable();

  EXPECT_EQ(collections.size(), test_collection_info.size());
  EXPECT_EQ(test_collection_info[0].collection_id, collections[0]->id);
  EXPECT_EQ(test_collection_info[0].collection_name, collections[0]->label);
  EXPECT_EQ(test_collection_info[0].preview_image_url,
            collections[0]->preview_image_url);
}

TEST_F(CustomizeChromePageHandlerTest, GetBackgroundImages) {
  std::vector<CollectionImage> test_collection_images;
  CollectionImage test_image;
  std::vector<std::string> attribution{"test1", "test2"};
  test_image.attribution = attribution;
  test_image.attribution_action_url = GURL("https://action.com");
  test_image.image_url = GURL("https://test_image.jpg");
  test_image.thumbnail_image_url = GURL("https://test_thumbnail.jpg");
  test_collection_images.push_back(test_image);
  ON_CALL(mock_ntp_background_service(), collection_images())
      .WillByDefault(ReturnRef(test_collection_images));

  std::vector<side_panel::mojom::CollectionImagePtr> images;
  base::MockCallback<CustomizeChromePageHandler::GetBackgroundImagesCallback>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&images));
  EXPECT_CALL(mock_ntp_background_service(), FetchCollectionImageInfo).Times(1);
  handler().GetBackgroundImages("test_id", callback.Get());
  ntp_background_service_observer().OnCollectionImagesAvailable();

  EXPECT_EQ(images.size(), test_collection_images.size());
  EXPECT_EQ(test_collection_images[0].attribution[0], images[0]->attribution_1);
  EXPECT_EQ(test_collection_images[0].attribution[1], images[0]->attribution_2);
  EXPECT_EQ(test_collection_images[0].attribution_action_url,
            images[0]->attribution_url);
  EXPECT_EQ(test_collection_images[0].image_url, images[0]->image_url);
  EXPECT_EQ(test_collection_images[0].thumbnail_image_url,
            images[0]->preview_image_url);
}

TEST_F(CustomizeChromePageHandlerTest, GetReplacementCollectionPreviewImage) {
  base::MockCallback<
      CustomizeChromePageHandler::GetReplacementCollectionPreviewImageCallback>
      callback;
  EXPECT_CALL(mock_ntp_background_service(),
              FetchReplacementCollectionPreviewImage)
      .Times(1);

  handler().GetReplacementCollectionPreviewImage("test_id", callback.Get());
}

TEST_F(CustomizeChromePageHandlerTest, SetDefaultColor) {
  EXPECT_CALL(mock_theme_service(), UseDefaultTheme).Times(1);
  EXPECT_CALL(mock_theme_service(), UseDeviceTheme(false)).Times(1);

  handler().SetDefaultColor();
}

TEST_F(CustomizeChromePageHandlerTest, RemoveBackgroundImage) {
  EXPECT_CALL(mock_ntp_custom_background_service_, ResetCustomBackgroundInfo)
      .Times(1);

  handler().RemoveBackgroundImage();
}

TEST_F(CustomizeChromePageHandlerTest, ChooseLocalCustomBackgroundSuccess) {
  bool success;
  base::MockCallback<
      CustomizeChromePageHandler::ChooseLocalCustomBackgroundCallback>
      callback;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(false));
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(SaveArg<0>(&success));
  EXPECT_CALL(mock_ntp_custom_background_service_,
              SelectLocalBackgroundImage(An<const base::FilePath&>()))
      .Times(1);
  ASSERT_EQ(0, user_action_tester().GetActionCount(
                   "NTPRicherPicker.Backgrounds.UploadConfirmed"));
  handler().ChooseLocalCustomBackground(callback.Get());
  EXPECT_TRUE(success);
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "NTPRicherPicker.Backgrounds.UploadConfirmed"));
}

TEST_F(CustomizeChromePageHandlerTest, ChooseLocalCustomBackgroundCancel) {
  bool success;
  base::MockCallback<
      CustomizeChromePageHandler::ChooseLocalCustomBackgroundCallback>
      callback;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(true));
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(SaveArg<0>(&success));
  ASSERT_EQ(0, user_action_tester().GetActionCount(
                   "NTPRicherPicker.Backgrounds.UploadCanceled"));
  handler().ChooseLocalCustomBackground(callback.Get());
  EXPECT_TRUE(!success);
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "NTPRicherPicker.Backgrounds.UploadCanceled"));
}

TEST_F(CustomizeChromePageHandlerTest, SetBackgroundImage) {
  EXPECT_CALL(mock_ntp_custom_background_service_, SetCustomBackgroundInfo)
      .Times(1);
  handler().SetBackgroundImage(
      "attribution1", "attribution2", GURL("https://attribution.com"),
      GURL("https://image.jpg"), GURL("https://thumbnail.jpg"), "collectionId");
}

TEST_F(CustomizeChromePageHandlerTest, OpenChromeWebStore) {
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 0);
  GURL url;
  EXPECT_CALL(mock_open_url_callback_, Run).Times(1).WillOnce(SaveArg<0>(&url));
  handler().OpenChromeWebStore();
  ASSERT_EQ(GURL("https://chromewebstore.google.com/category/themes"), url);
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 1);

  ASSERT_EQ(
      histogram_tester().GetBucketCount("NewTabPage.ChromeWebStoreOpen",
                                        NtpChromeWebStoreOpen::kAppearance),
      1);
}

TEST_F(CustomizeChromePageHandlerTest, OpenThirdPartyThemePage) {
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 0);
  GURL url;
  EXPECT_CALL(mock_open_url_callback_, Run).Times(1).WillOnce(SaveArg<0>(&url));
  handler().OpenThirdPartyThemePage("foo");
  ASSERT_EQ(GURL("https://chromewebstore.google.com/detail/foo"), url);
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 1);
  ASSERT_EQ(
      histogram_tester().GetBucketCount("NewTabPage.ChromeWebStoreOpen",
                                        NtpChromeWebStoreOpen::kCollections),
      1);
}

TEST_F(CustomizeChromePageHandlerTest, OpenChromeWebStoreCategoryPage) {
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 0);
  GURL url;
  EXPECT_CALL(mock_open_url_callback_, Run).Times(1).WillOnce(SaveArg<0>(&url));
  handler().OpenChromeWebStoreCategoryPage(
      side_panel::mojom::ChromeWebStoreCategory::kWorkflowPlanning);

  ASSERT_EQ(
      GURL("https://chromewebstore.google.com/category/extensions/"
           "productivity/workflow?utm_source=chromeSidebarExtensionCards"),
      url);
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 1);

  ASSERT_EQ(histogram_tester().GetBucketCount(
                "NewTabPage.ChromeWebStoreOpen",
                NtpChromeWebStoreOpen::kWorkflowPlanningCategoryPage),
            1);
}

TEST_F(CustomizeChromePageHandlerTest, OpenChromeWebStoreCollectionPage) {
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 0);
  GURL url;
  EXPECT_CALL(mock_open_url_callback_, Run).Times(1).WillOnce(SaveArg<0>(&url));
  handler().OpenChromeWebStoreCollectionPage(
      side_panel::mojom::ChromeWebStoreCollection::kWritingEssentials);
  ASSERT_EQ(GURL("https://chromewebstore.google.com/collection/"
                 "writing_essentials?utm_source=chromeSidebarExtensionCards"),
            url);
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 1);

  ASSERT_EQ(histogram_tester().GetBucketCount(
                "NewTabPage.ChromeWebStoreOpen",
                NtpChromeWebStoreOpen::kWritingEssentialsCollectionPage),
            1);
}

TEST_F(CustomizeChromePageHandlerTest, OpenNtpManagedByPage) {
  GURL url;
  EXPECT_CALL(mock_open_url_callback_, Run).Times(1).WillOnce(SaveArg<0>(&url));
  handler().OpenNtpManagedByPage();

  EXPECT_EQ(GURL(chrome::kBrowserSettingsSearchEngineURL), url);
}

TEST_F(CustomizeChromePageHandlerTest, OpenChromeWebStoreHomePage) {
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 0);
  GURL url;
  EXPECT_CALL(mock_open_url_callback_, Run).Times(1).WillOnce(SaveArg<0>(&url));
  handler().OpenChromeWebStoreHomePage();
  ASSERT_EQ(
      GURL("https://"
           "chromewebstore.google.com/?utm_source=chromeSidebarExtensionCards"),
      url);
  histogram_tester().ExpectTotalCount("NewTabPage.ChromeWebStoreOpen", 1);

  ASSERT_EQ(histogram_tester().GetBucketCount("NewTabPage.ChromeWebStoreOpen",
                                              NtpChromeWebStoreOpen::kHomePage),
            1);
}

TEST_F(CustomizeChromePageHandlerTest, SetDailyRefreshCollectionId) {
  EXPECT_CALL(mock_ntp_custom_background_service_, SetCustomBackgroundInfo)
      .Times(1);
  handler().SetDailyRefreshCollectionId("test_id");
}

TEST_F(CustomizeChromePageHandlerTest, SetFollowDeviceTheme_On) {
  EXPECT_CALL(mock_theme_service(), UseDeviceTheme(true)).Times(1);

  handler().SetFollowDeviceTheme(true);
}

TEST_F(CustomizeChromePageHandlerTest, SetUseDeviceTheme_Off) {
  EXPECT_CALL(mock_theme_service(), UseDeviceTheme(false)).Times(1);

  handler().SetFollowDeviceTheme(false);
}

TEST_F(CustomizeChromePageHandlerTest, ScrollToSection) {
  CustomizeChromeSection section;
  EXPECT_CALL(mock_page_, ScrollToSection)
      .Times(1)
      .WillOnce(SaveArg<0>(&section));

  handler().ScrollToSection(CustomizeChromeSection::kAppearance);
  mock_page_.FlushForTesting();

  EXPECT_EQ(CustomizeChromeSection::kAppearance, section);
}

// Ensures that url's are correctly mapped to their NewTabPage type.
// Does not include test for `side_panel::mojom::NewTabPageType::kExtension`
// See `CustomizeChromeInteractiveTest.FooterSectionForExtensionNtp` for
// confirming Customize Chrome shows the footer section for Extension NTPs.
TEST_F(CustomizeChromePageHandlerTest, AttachedTabStateUpdated) {
  std::vector<std::pair<side_panel::mojom::NewTabPageType, GURL>>
      ntp_types_and_urls = {
          {side_panel::mojom::NewTabPageType::kNone,
           GURL("chrome-extension://someinvaldextension/index.html")},
          {side_panel::mojom::NewTabPageType::kFirstPartyWebUI,
           GURL(chrome::kChromeUINewTabPageURL)},
          {side_panel::mojom::NewTabPageType::kThirdPartyWebUI,
           GURL(chrome::kChromeUINewTabPageThirdPartyURL)},
          {side_panel::mojom::NewTabPageType::kIncognito,
           GURL(chrome::kChromeUINewTabURL)},
          {side_panel::mojom::NewTabPageType::kGuestMode,
           GURL(chrome::kChromeUINewTabURL)}};

  for (const auto& ntp_type_and_url : ntp_types_and_urls) {
    if (ntp_type_and_url.first ==
        side_panel::mojom::NewTabPageType::kGuestMode) {
      profile().SetGuestSession(true);
    }

    side_panel::mojom::NewTabPageType source_tab;
    EXPECT_CALL(mock_page_, AttachedTabStateUpdated)
        .Times(1)
        .WillOnce(SaveArg<0>(&source_tab));
    handler().AttachedTabStateUpdated(ntp_type_and_url.second);
    mock_page_.FlushForTesting();
    EXPECT_EQ(ntp_type_and_url.first, source_tab);
  }
}

TEST_F(CustomizeChromePageHandlerTest, ScrollToUnspecifiedSection) {
  EXPECT_CALL(mock_page_, ScrollToSection).Times(0);

  handler().ScrollToSection(CustomizeChromeSection::kUnspecified);
  mock_page_.FlushForTesting();
}

TEST_F(CustomizeChromePageHandlerTest, UpdateScrollToSection) {
  CustomizeChromeSection section;
  EXPECT_CALL(mock_page_, ScrollToSection)
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&section));

  handler().ScrollToSection(CustomizeChromeSection::kAppearance);
  handler().UpdateScrollToSection();
  mock_page_.FlushForTesting();

  EXPECT_EQ(CustomizeChromeSection::kAppearance, section);
}

// Tests that SetToolChipsVisible sets the pref to the given value.
TEST_F(CustomizeChromePageHandlerTest, SetToolChipsVisible) {
  profile().GetPrefs()->SetBoolean(prefs::kNtpToolChipsVisible, false);
  handler().SetToolChipsVisible(true);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(profile().GetPrefs()->GetBoolean(prefs::kNtpToolChipsVisible));
}

// Tests that UpdateToolChipsSettings calls the page with SetToolsSettings
TEST_F(CustomizeChromePageHandlerTest, UpdateToolChipsSettings) {
  bool visible;

  EXPECT_CALL(mock_page_, SetToolsSettings)
      .Times(1)
      .WillOnce([&visible](bool visible_arg) { visible = visible_arg; });

  profile().GetPrefs()->SetBoolean(prefs::kNtpToolChipsVisible, true);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(visible);
  EXPECT_TRUE(profile().GetPrefs()->GetBoolean(prefs::kNtpToolChipsVisible));
}

class CustomizeChromePageHandlerWallpaperSearchTest
    : public CustomizeChromePageHandlerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  CustomizeChromePageHandlerWallpaperSearchTest() {
    std::vector<base::test::FeatureRef> kWallpaperSearchFeatures = {
        ntp_features::kCustomizeChromeWallpaperSearch,
        optimization_guide::features::kOptimizationGuideModelExecution};

    if (WallpaperSearchEnabled()) {
      scoped_feature_list_.InitWithFeatures(kWallpaperSearchFeatures, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, kWallpaperSearchFeatures);
    }
  }
  bool WallpaperSearchEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(CustomizeChromePageHandlerWallpaperSearchTest,
       ChooseLocalCustomBackground) {
  base::MockCallback<
      CustomizeChromePageHandler::ChooseLocalCustomBackgroundCallback>
      callback;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(false));

  if (WallpaperSearchEnabled()) {
    EXPECT_CALL(mock_theme_service(), UseDefaultTheme).Times(0);
  } else {
    EXPECT_CALL(mock_theme_service(), UseDefaultTheme).Times(1);
  }

  handler().ChooseLocalCustomBackground(callback.Get());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CustomizeChromePageHandlerWallpaperSearchTest,
                         ::testing::Bool());

class CustomizeChromePageHandlerWithModulesTest
    : public CustomizeChromePageHandlerTest {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{ntp_features::
                                  kNtpMostRelevantTabResumptionModule},
        /*disabled_features=*/{});
    CustomizeChromePageHandlerTest::SetUp();
  }
};

TEST_F(CustomizeChromePageHandlerWithModulesTest, SetModulesSettings) {
  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  bool managed;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(1)
      .WillRepeatedly([&modules_settings, &managed](
                          std::vector<side_panel::mojom::ModuleSettingsPtr>
                              modules_settings_arg,
                          bool managed_arg, bool visible_arg) {
        modules_settings = std::move(modules_settings_arg);
        managed = managed_arg;
      });

  profile().GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, true);
  mock_page_.FlushForTesting();

  EXPECT_FALSE(managed);
  EXPECT_EQ(2u, modules_settings.size());
  const auto& tab_resumption_settings = modules_settings[0];
  EXPECT_EQ(ntp_modules::kMostRelevantTabResumptionModuleId,
            tab_resumption_settings->id);
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE),
            tab_resumption_settings->name);
  EXPECT_EQ(std::nullopt, tab_resumption_settings->description);
  EXPECT_TRUE(tab_resumption_settings->enabled);
  const auto& microsoft_auth_settings = modules_settings[1];
  EXPECT_EQ(ntp_modules::kMicrosoftAuthenticationModuleId,
            microsoft_auth_settings->id);
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_NTP_MODULES_MICROSOFT_AUTHENTICATION_NAME),
      microsoft_auth_settings->name);
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_NTP_MICROSOFT_AUTHENTICATION_SIDE_PANEL_DESCRIPTION),
            microsoft_auth_settings->description);
  EXPECT_TRUE(microsoft_auth_settings->enabled);
}

TEST_F(CustomizeChromePageHandlerWithModulesTest, SetModulesVisible_True) {
  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  bool visible;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(1)
      .WillRepeatedly([&modules_settings, &visible](
                          std::vector<side_panel::mojom::ModuleSettingsPtr>
                              modules_settings_arg,
                          bool managed_arg, bool visible_arg) {
        modules_settings = std::move(modules_settings_arg);
        visible = visible_arg;
      });

  handler().SetModulesVisible(true);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(visible);
}

TEST_F(CustomizeChromePageHandlerWithModulesTest, SetModulesVisible_False) {
  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  bool visible;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(1)
      .WillRepeatedly([&modules_settings, &visible](
                          std::vector<side_panel::mojom::ModuleSettingsPtr>
                              modules_settings_arg,
                          bool managed_arg, bool visible_arg) {
        modules_settings = std::move(modules_settings_arg);
        visible = visible_arg;
      });

  handler().SetModulesVisible(false);
  mock_page_.FlushForTesting();

  EXPECT_FALSE(visible);
}

TEST_F(CustomizeChromePageHandlerWithModulesTest, SetModuleDisabled) {
  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(1)
      .WillRepeatedly(
          [&modules_settings](std::vector<side_panel::mojom::ModuleSettingsPtr>
                                  modules_settings_arg,
                              bool managed_arg, bool visible_arg) {
            modules_settings = std::move(modules_settings_arg);
          });

  const std::string kTabResumptionId(
      ntp_modules::kMostRelevantTabResumptionModuleId);
  handler().SetModuleDisabled(kTabResumptionId, true);
  mock_page_.FlushForTesting();

  EXPECT_EQ(2u, modules_settings.size());
  const auto& tab_resumption_settings = modules_settings[0];
  EXPECT_EQ(kTabResumptionId, tab_resumption_settings->id);
  EXPECT_FALSE(tab_resumption_settings->enabled);
  const auto& disabled_module_ids =
      profile().GetPrefs()->GetList(prefs::kNtpDisabledModules);
  EXPECT_EQ(kTabResumptionId, disabled_module_ids.front().GetString());
  const auto& microsoft_auth_settings = modules_settings[1];
  EXPECT_EQ(ntp_modules::kMicrosoftAuthenticationModuleId,
            microsoft_auth_settings->id);
  EXPECT_TRUE(microsoft_auth_settings->enabled);
}

TEST_F(CustomizeChromePageHandlerWithModulesTest,
       SetModuleHiddenInCustomizeChrome) {
  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(1)
      .WillRepeatedly(
          [&modules_settings](std::vector<side_panel::mojom::ModuleSettingsPtr>
                                  modules_settings_arg,
                              bool managed_arg, bool visible_arg) {
            modules_settings = std::move(modules_settings_arg);
          });

  base::Value::List hidden_modules_list;
  hidden_modules_list.Append(ntp_modules::kMostRelevantTabResumptionModuleId);

  profile().GetPrefs()->SetList(prefs::kNtpHiddenModules,
                                std::move(hidden_modules_list));
  mock_page_.FlushForTesting();

  EXPECT_EQ(2u, modules_settings.size());
  const auto& tab_resumption_settings = modules_settings[0];
  EXPECT_FALSE(tab_resumption_settings->visible);
  const auto& microsoft_auth_settings = modules_settings[1];
  EXPECT_TRUE(microsoft_auth_settings->visible);
}

class CustomizeChromePageHandlerWithModulesVisibilityTest
    : public CustomizeChromePageHandlerWithModulesTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool ModulesVisible() const { return GetParam(); }
};

TEST_P(CustomizeChromePageHandlerWithModulesVisibilityTest, SetModulesVisible) {
  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  bool visible;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(1)
      .WillRepeatedly([&modules_settings, &visible](
                          std::vector<side_panel::mojom::ModuleSettingsPtr>
                              modules_settings_arg,
                          bool managed_arg, bool visible_arg) {
        modules_settings = std::move(modules_settings_arg);
        visible = visible_arg;
      });

  handler().SetModulesVisible(ModulesVisible());
  mock_page_.FlushForTesting();

  EXPECT_EQ(ModulesVisible(), visible);
}

INSTANTIATE_TEST_SUITE_P(All,
                         CustomizeChromePageHandlerWithModulesVisibilityTest,
                         ::testing::Bool());

class CustomizeChromePageHandlerWithTemplateURLServiceTest
    : public CustomizeChromePageHandlerTest {
 public:
  CustomizeChromePageHandlerWithTemplateURLServiceTest()
      : factory_util_(profile_.get()) {}
  ~CustomizeChromePageHandlerWithTemplateURLServiceTest() override = default;

  void SetUp() override {
    CustomizeChromePageHandlerTest::SetUp();
    factory_util_.VerifyLoad();
    AddSearchProviders();
  }

  static constexpr char16_t kFirstPartyShortName[] = u"first party";
  static constexpr char16_t kThirdPartyShortName[] = u"third party";
  static constexpr char kFirstPartyDomain[] = "{google:baseURL}";
  static constexpr char kFirstPartySuggestDomain[] = "{google:baseSuggestURL}";
  static constexpr char kThirdPartyDomain[] = "www.third_party.com";

  void AddSearchProviders() {
    {  // third party
      TemplateURLData data;
      data.SetURL(std::string(kThirdPartyDomain) + "/search?q={searchTerms}");
      data.suggestions_url =
          std::string(kThirdPartyDomain) + "/search?q={searchTerms}";
      data.image_url = std::string(kThirdPartyDomain) + "/searchbyimage/upload";
      data.image_translate_url =
          std::string(kThirdPartyDomain) + "/searchbyimage/upload?translate";
      data.new_tab_url = std::string(kThirdPartyDomain) + "/_/chrome/newtab";
      data.contextual_search_url =
          std::string(kThirdPartyDomain) + "/_/contextualsearch";
      data.alternate_urls.push_back(std::string(kThirdPartyDomain) +
                                    "/s#q={searchTerms}");
      data.SetShortName(kThirdPartyShortName);
      third_party_url_ =
          factory_util_.model()->Add(std::make_unique<TemplateURL>(data));
    }

    {  // first party
      TemplateURLData data;
      data.SetURL(std::string(kFirstPartyDomain) + "search?q={searchTerms}");
      data.suggestions_url =
          std::string(kFirstPartySuggestDomain) + "search?q={searchTerms}";
      data.image_url = std::string(kFirstPartyDomain) + "searchbyimage/upload";
      data.image_translate_url =
          std::string(kFirstPartyDomain) + "searchbyimage/upload?translate";
      data.new_tab_url = std::string(kFirstPartyDomain) + "_/chrome/newtab";
      data.contextual_search_url =
          std::string(kFirstPartyDomain) + "_/contextualsearch";
      data.alternate_urls.push_back(std::string(kFirstPartyDomain) +
                                    "s#q={searchTerms}");
      data.SetShortName(kFirstPartyShortName);
      first_party_url_ =
          factory_util_.model()->Add(std::make_unique<TemplateURL>(data));
    }
  }

  void SetFirstPartyDefault() {
    factory_util_.model()->SetUserSelectedDefaultSearchProvider(
        first_party_url_);
  }

  void SetThirdPartyDefault() {
    factory_util_.model()->SetUserSelectedDefaultSearchProvider(
        third_party_url_);
  }

 private:
  TemplateURLServiceFactoryTestUtil factory_util_;
  raw_ptr<TemplateURL> first_party_url_ = nullptr;
  raw_ptr<TemplateURL> third_party_url_ = nullptr;
};

TEST_F(CustomizeChromePageHandlerWithTemplateURLServiceTest,
       NtpManagedByNameUpdated) {
  mock_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_page_);

  std::string name;
  std::string description;
  EXPECT_CALL(mock_page_, NtpManagedByNameUpdated)
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&name), SaveArg<1>(&description)));
  SetFirstPartyDefault();
  mock_page_.FlushForTesting();
  EXPECT_EQ(std::string(), name);
  EXPECT_EQ(std::string(), description);

  mock_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_page_);

  EXPECT_CALL(mock_page_, NtpManagedByNameUpdated)
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&name), SaveArg<1>(&description)));
  SetThirdPartyDefault();
  mock_page_.FlushForTesting();
  EXPECT_EQ(std::string(base::UTF16ToUTF8(kThirdPartyShortName)), name);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_NTP_MANAGED_BY_SEARCH_ENGINE),
            description);
}
