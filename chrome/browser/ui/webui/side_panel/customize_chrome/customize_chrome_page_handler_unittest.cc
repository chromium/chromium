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
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/background/ntp_background_data.h"
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
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
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
#include "ui/native_theme/native_theme.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace {

using testing::_;
using testing::An;
using testing::DoAll;
using testing::Invoke;
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
              (bool custom_links_enabled, bool visible));
  MOCK_METHOD(void, SetTheme, (side_panel::mojom::ThemePtr));
  MOCK_METHOD(void,
              ScrollToSection,
              (side_panel::mojom::CustomizeChromeSection));
  MOCK_METHOD(void, AttachedTabStateUpdated, (bool));
  MOCK_METHOD(void, NtpManagedByNameUpdated, (const std::string&));

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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : NtpBackgroundService(url_loader_factory) {}
  MOCK_CONST_METHOD0(collection_info, std::vector<CollectionInfo>&());
  MOCK_CONST_METHOD0(collection_images, std::vector<CollectionImage>&());
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      NtpBackgroundServiceFactory::GetInstance(),
      base::BindRepeating(
          [](scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<MockNtpBackgroundService>>(
                url_loader_factory);
          },
          url_loader_factory));
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
      : profile_(
            MakeTestingProfile(test_url_loader_factory_.GetSafeWeakWrapper())),
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
    const std::vector<std::pair<const std::string, int>> module_id_names = {
        {"tab_resumption", IDS_NTP_TAB_RESUMPTION_TITLE}};
    handler_ = std::make_unique<CustomizeChromePageHandler>(
        mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>(),
        mock_page_.BindAndGetRemote(), &mock_ntp_custom_background_service_,
        web_contents_, module_id_names, mock_open_url_callback_.Get());
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), ntp_background_service_observer_);
    EXPECT_EQ(handler_.get(), ntp_custom_background_service_observer_);

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams browser_params(profile_.get(), true);
    browser_params.type = Browser::TYPE_NORMAL;
    browser_params.window = browser_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(browser_params));

    scoped_feature_list_.Reset();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    browser_->tab_strip_model()->CloseAllTabs();
    browser_.reset();
    browser_window_.reset();
    test_url_loader_factory_.ClearResponses();
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
  std::unique_ptr<TestBrowserWindow> browser_window_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::MockRepeatingCallback<void(const GURL& gurl)> mock_open_url_callback_;
  std::unique_ptr<CustomizeChromePageHandler> handler_;
  raw_ptr<NtpCustomBackgroundServiceObserver>
    ntp_custom_background_service_observer_;
  raw_ptr<NtpBackgroundServiceObserver> ntp_background_service_observer_;
};

TEST_F(CustomizeChromePageHandlerTest, SetMostVisitedSettings) {
  bool custom_links_enabled;
  bool visible;
  EXPECT_CALL(mock_page_, SetMostVisitedSettings)
      .Times(4)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&custom_links_enabled), SaveArg<1>(&visible)));

  profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);
  profile().GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);

  histogram_tester().ExpectTotalCount("NewTabPage.CustomizeShortcutAction", 0);
  EXPECT_FALSE(
      profile().GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles));
  EXPECT_FALSE(
      profile().GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));

  handler().SetMostVisitedSettings(/*custom_links_enabled=*/false,
                                   /*visible=*/true);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(
      profile().GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles));
  EXPECT_TRUE(
      profile().GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
  histogram_tester().ExpectTotalCount("NewTabPage.CustomizeShortcutAction", 2);
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
  void UpdateTheme() {
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
  }
};

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetTheme) {
  side_panel::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg<0>(&theme));
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
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);

  UpdateTheme();
  mock_page_.FlushForTesting();

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
  side_panel::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg(&theme));
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.daily_refresh_enabled = true;
  custom_background.collection_id = "test_collection";
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));

  UpdateTheme();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_TRUE(theme->background_image->daily_refresh_enabled);
  EXPECT_EQ("test_collection", theme->background_image->collection_id);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetUploadedImage) {
  side_panel::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg<0>(&theme));
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.is_uploaded_image = true;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service(), UsingDefaultTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingSystemTheme())
      .WillByDefault(Return(false));

  UpdateTheme();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  ASSERT_TRUE(theme->background_image->is_uploaded_image);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetWallpaperSearchImage) {
  side_panel::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg<0>(&theme));
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

  UpdateTheme();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_TRUE(theme->background_image->is_uploaded_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  EXPECT_EQ(token, theme->background_image->local_background_id);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetThirdPartyTheme) {
  side_panel::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg<0>(&theme));
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

  UpdateTheme();
  mock_page_.FlushForTesting();

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
  EXPECT_CALL(mock_ntp_background_service(), FetchCollectionInfo).Times(1);
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
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg<0>(&images));
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
  EXPECT_CALL(mock_theme_service(), UseDefaultTheme).Times(1);
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
  ASSERT_EQ(GURL("https://chrome.google.com/webstore?category=theme"), url);
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
  ASSERT_EQ(GURL("https://chrome.google.com/webstore/detail/foo"), url);
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
  side_panel::mojom::CustomizeChromeSection section;
  EXPECT_CALL(mock_page_, ScrollToSection)
      .Times(1)
      .WillOnce(SaveArg<0>(&section));

  handler().ScrollToSection(CustomizeChromeSection::kAppearance);
  mock_page_.FlushForTesting();

  EXPECT_EQ(side_panel::mojom::CustomizeChromeSection::kAppearance, section);
}

TEST_F(CustomizeChromePageHandlerTest, AttachedTabStateUpdated) {
  bool kIsSourceTabFirstPartyNtpValue = false;

  bool isSourceTabFirstPartyNtp;
  EXPECT_CALL(mock_page_, AttachedTabStateUpdated)
      .Times(1)
      .WillOnce(SaveArg<0>(&isSourceTabFirstPartyNtp));

  handler().AttachedTabStateUpdated(kIsSourceTabFirstPartyNtpValue);
  mock_page_.FlushForTesting();

  EXPECT_EQ(kIsSourceTabFirstPartyNtpValue, isSourceTabFirstPartyNtp);
}

TEST_F(CustomizeChromePageHandlerTest, ScrollToUnspecifiedSection) {
  EXPECT_CALL(mock_page_, ScrollToSection).Times(0);

  handler().ScrollToSection(CustomizeChromeSection::kUnspecified);
  mock_page_.FlushForTesting();
}

TEST_F(CustomizeChromePageHandlerTest, UpdateScrollToSection) {
  side_panel::mojom::CustomizeChromeSection section;
  EXPECT_CALL(mock_page_, ScrollToSection)
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&section));

  handler().ScrollToSection(CustomizeChromeSection::kAppearance);
  handler().UpdateScrollToSection();
  mock_page_.FlushForTesting();

  EXPECT_EQ(side_panel::mojom::CustomizeChromeSection::kAppearance, section);
}

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
  bool visible;
  EXPECT_CALL(mock_page_, SetModulesSettings)
      .Times(2)
      .WillRepeatedly(
          Invoke([&modules_settings, &managed, &visible](
                     std::vector<side_panel::mojom::ModuleSettingsPtr>
                         modules_settings_arg,
                     bool managed_arg, bool visible_arg) {
            modules_settings = std::move(modules_settings_arg);
            managed = managed_arg;
            visible = visible_arg;
          }));

  constexpr char kTabResumptionId[] = "tab_resumption";
  profile().GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, true);
  auto disabled_module_ids = base::Value::List();
  disabled_module_ids.Append(kTabResumptionId);
  profile().GetPrefs()->SetList(prefs::kNtpDisabledModules,
                                std::move(disabled_module_ids));
  mock_page_.FlushForTesting();

  EXPECT_TRUE(visible);
  EXPECT_FALSE(managed);
  EXPECT_EQ(1u, modules_settings.size());
  EXPECT_EQ(kTabResumptionId, modules_settings[0]->id);
  EXPECT_FALSE(modules_settings[0]->enabled);
}

TEST_F(CustomizeChromePageHandlerWithModulesTest, SetModulesVisible) {
  profile().GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, false);
  handler().SetModulesVisible(true);

  EXPECT_CALL(mock_page_, SetModulesSettings).Times(2);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(profile().GetPrefs()->GetBoolean(prefs::kNtpModulesVisible));
}

TEST_F(CustomizeChromePageHandlerWithModulesTest, SetModuleDisabled) {
  const std::string kDriveModuleId = "drive";
  handler().SetModuleDisabled(kDriveModuleId, true);
  const auto& disabled_module_ids =
      profile().GetPrefs()->GetList(prefs::kNtpDisabledModules);

  EXPECT_CALL(mock_page_, SetModulesSettings).Times(1);
  mock_page_.FlushForTesting();

  EXPECT_EQ(1u, disabled_module_ids.size());
  EXPECT_EQ(kDriveModuleId, disabled_module_ids.front().GetString());
}

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
  EXPECT_CALL(mock_page_, NtpManagedByNameUpdated)
      .Times(1)
      .WillOnce(SaveArg<0>(&name));
  SetFirstPartyDefault();
  mock_page_.FlushForTesting();
  EXPECT_EQ(std::string(), name);

  mock_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_page_);

  EXPECT_CALL(mock_page_, NtpManagedByNameUpdated)
      .Times(1)
      .WillOnce(SaveArg<0>(&name));
  SetThirdPartyDefault();
  mock_page_.FlushForTesting();
  EXPECT_EQ(std::string(base::UTF16ToUTF8(kThirdPartyShortName)), name);
}
