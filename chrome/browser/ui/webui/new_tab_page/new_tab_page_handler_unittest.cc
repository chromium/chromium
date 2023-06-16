// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/customize_chrome/customize_chrome_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/promos/promo_data.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/new_tab_page/promos/promo_service_factory.h"
#include "chrome/browser/new_tab_page/promos/promo_service_observer.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Optional;
using testing::SaveArg;

class MockPage : public new_tab_page::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<new_tab_page::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, SetTheme, (new_tab_page::mojom::ThemePtr));
  MOCK_METHOD(void,
              SetDisabledModules,
              (bool, const std::vector<std::string>&));
  MOCK_METHOD(void, SetModulesFreVisibility, (bool));
  MOCK_METHOD(void, SetCustomizeChromeSidePanelVisibility, (bool));
  MOCK_METHOD(void, SetPromo, (new_tab_page::mojom::PromoPtr));

  mojo::Receiver<new_tab_page::mojom::Page> receiver_{this};
};

class MockLogoService : public search_provider_logos::LogoService {
 public:
  MOCK_METHOD(void, GetLogo, (search_provider_logos::LogoCallbacks, bool));
  MOCK_METHOD(void, GetLogo, (search_provider_logos::LogoObserver*));
};

class MockColorProviderSource : public ui::ColorProviderSource {
 public:
  MockColorProviderSource() { color_provider_.GenerateColorMap(); }
  MockColorProviderSource(const MockColorProviderSource&) = delete;
  MockColorProviderSource& operator=(const MockColorProviderSource&) = delete;
  ~MockColorProviderSource() override = default;

  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  void SetColor(ui::ColorId id, SkColor color) {
    color_provider_.SetColorForTesting(id, color);
  }

 protected:
  ui::ColorProviderManager::Key GetColorProviderKey() const override {
    return ui::ColorProviderManager::Key();
  }

 private:
  ui::ColorProvider color_provider_;
};

class MockThemeProvider : public ui::ThemeProvider {
 public:
  MOCK_CONST_METHOD1(GetImageSkiaNamed, gfx::ImageSkia*(int));
  MOCK_CONST_METHOD1(GetColor, SkColor(int));
  MOCK_CONST_METHOD1(GetTint, color_utils::HSL(int));
  MOCK_CONST_METHOD1(GetDisplayProperty, int(int));
  MOCK_CONST_METHOD0(ShouldUseNativeFrame, bool());
  MOCK_CONST_METHOD1(HasCustomImage, bool(int));
  MOCK_CONST_METHOD2(GetRawData,
                     base::RefCountedMemory*(int, ui::ResourceScaleFactor));
};

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD(void, RefreshBackgroundIfNeeded, ());
  MOCK_METHOD(absl::optional<CustomBackground>, GetCustomBackground, ());
  MOCK_METHOD(void, AddObserver, (NtpCustomBackgroundServiceObserver*));
};

class MockThemeService : public ThemeService {
 public:
  MockThemeService() : ThemeService(nullptr, theme_helper_) {}
  MOCK_CONST_METHOD0(GetThemeID, std::string());
  MOCK_CONST_METHOD0(UsingAutogeneratedTheme, bool());
  MOCK_CONST_METHOD0(UsingDefaultTheme, bool());
  MOCK_CONST_METHOD0(UsingExtensionTheme, bool());
  MOCK_METHOD(void, AddObserver, (ThemeServiceObserver*));

 private:
  ThemeHelper theme_helper_;
};

class MockPromoService : public PromoService {
 public:
  MockPromoService() : PromoService(nullptr, nullptr) {}
  MOCK_METHOD(const absl::optional<PromoData>&,
              promo_data,
              (),
              (const, override));
  MOCK_METHOD(void, AddObserver, (PromoServiceObserver*), (override));
  MOCK_METHOD(void, Refresh, (), (override));
};

class MockCustomizeChromeTabHelper : public CustomizeChromeTabHelper {
 public:
  static MockCustomizeChromeTabHelper* CreateForWebContents(
      content::WebContents* contents) {
    DCHECK(contents);
    DCHECK(!contents->GetUserData(UserDataKey()));
    contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new MockCustomizeChromeTabHelper(contents)));
    return static_cast<MockCustomizeChromeTabHelper*>(
        contents->GetUserData(UserDataKey()));
  }

  MOCK_METHOD(void,
              SetCustomizeChromeSidePanelVisible,
              (bool, CustomizeChromeSection),
              (override));

 private:
  explicit MockCustomizeChromeTabHelper(content::WebContents* web_contents)
      : CustomizeChromeTabHelper(web_contents) {}
};

class MockCustomizeChromeFeaturePromoHelper
    : public CustomizeChromeFeaturePromoHelper {
 public:
  MOCK_METHOD(void,
              RecordCustomizeChromeFeatureUsage,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void,
              MaybeShowCustomizeChromeFeaturePromo,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void,
              CloseCustomizeChromeFeaturePromo,
              (content::WebContents*),
              (override));
  MOCK_METHOD(bool,
              IsSigninModalDialogOpen,
              (content::WebContents*),
              (override));

  ~MockCustomizeChromeFeaturePromoHelper() override = default;
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      PromoServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockPromoService>>();
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  return profile;
}

}  // namespace

class NewTabPageHandlerTest : public testing::Test {
 public:
  NewTabPageHandlerTest()
      : profile_(MakeTestingProfile(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))),
        mock_ntp_custom_background_service_(profile_.get()),
        mock_promo_service_(*static_cast<MockPromoService*>(
            PromoServiceFactory::GetForProfile(profile_.get()))),
        web_contents_(factory_.CreateWebContents(profile_.get())),
        mock_customize_chrome_feature_promo_helper_(
            new MockCustomizeChromeFeaturePromoHelper()),
        mock_customize_chrome_feature_promo_helper_ptr_(
            std::unique_ptr<MockCustomizeChromeFeaturePromoHelper>(
                mock_customize_chrome_feature_promo_helper_)),
        mock_customize_chrome_tab_helper_(
            MockCustomizeChromeTabHelper::CreateForWebContents(
                web_contents_.get())) {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

  ~NewTabPageHandlerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_theme_service_, AddObserver)
        .Times(1)
        .WillOnce(testing::SaveArg<0>(&theme_service_observer_));
    EXPECT_CALL(mock_ntp_custom_background_service_, AddObserver)
        .Times(1)
        .WillOnce(
            testing::SaveArg<0>(&ntp_custom_background_service_observer_));
    EXPECT_CALL(*mock_promo_service_, AddObserver)
        .Times(1)
        .WillOnce(testing::SaveArg<0>(&promo_service_observer_));
    EXPECT_CALL(mock_page_, SetTheme).Times(1);
    EXPECT_CALL(mock_ntp_custom_background_service_, RefreshBackgroundIfNeeded)
        .Times(1);
    webui::SetThemeProviderForTesting(&mock_theme_provider_);
    web_contents_->SetColorProviderSource(&mock_color_provider_source_);
    const std::vector<std::pair<const std::string, int>> module_id_names = {
        {"recipe_tasks", IDS_NTP_MODULES_RECIPE_TASKS_SENTENCE}};
    EXPECT_FALSE(
        mock_customize_chrome_tab_helper_->IsCustomizeChromeEntryShowing());
    handler_ = std::make_unique<NewTabPageHandler>(
        mojo::PendingReceiver<new_tab_page::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get(),
        &mock_ntp_custom_background_service_, &mock_theme_service_,
        &mock_logo_service_, web_contents_,
        std::move(mock_customize_chrome_feature_promo_helper_ptr_),
        base::Time::Now(), module_id_names);
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), theme_service_observer_);
    EXPECT_EQ(handler_.get(), ntp_custom_background_service_observer_);
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
    testing::Mock::VerifyAndClearExpectations(
        &mock_ntp_custom_background_service_);
  }

  new_tab_page::mojom::DoodlePtr GetDoodle(
      const search_provider_logos::EncodedLogo& logo) {
    search_provider_logos::EncodedLogoCallback on_cached_encoded_logo_available;
    EXPECT_CALL(mock_logo_service_, GetLogo(testing::_, testing::_))
        .Times(1)
        .WillOnce(
            testing::Invoke([&on_cached_encoded_logo_available](
                                search_provider_logos::LogoCallbacks callbacks,
                                bool for_webui_ntp) {
              on_cached_encoded_logo_available =
                  std::move(callbacks.on_cached_encoded_logo_available);
            }));
    base::MockCallback<NewTabPageHandler::GetDoodleCallback> callback;
    new_tab_page::mojom::DoodlePtr doodle;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce(
            testing::Invoke([&doodle](new_tab_page::mojom::DoodlePtr arg) {
              doodle = std::move(arg);
            }));
    handler_->GetDoodle(callback.Get());

    std::move(on_cached_encoded_logo_available)
        .Run(search_provider_logos::LogoCallbackReason::DETERMINED,
             absl::make_optional(logo));

    return doodle;
  }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNtpCustomBackgroundService>
      mock_ntp_custom_background_service_;
  testing::NiceMock<MockThemeService> mock_theme_service_;
  MockLogoService mock_logo_service_;
  MockColorProviderSource mock_color_provider_source_;
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  testing::NiceMock<MockThemeProvider> mock_theme_provider_;
  const raw_ref<MockPromoService> mock_promo_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  // Pointer to mock that will eventually be solely owned by the handler.
  raw_ptr<MockCustomizeChromeFeaturePromoHelper, DanglingUntriaged>
      mock_customize_chrome_feature_promo_helper_;
  std::unique_ptr<MockCustomizeChromeFeaturePromoHelper>
      mock_customize_chrome_feature_promo_helper_ptr_;
  raw_ptr<MockCustomizeChromeTabHelper> mock_customize_chrome_tab_helper_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<NewTabPageHandler> handler_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION ThemeServiceObserver* theme_service_observer_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION NtpCustomBackgroundServiceObserver*
      ntp_custom_background_service_observer_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION PromoServiceObserver* promo_service_observer_;

 private:
  raw_ptr<MockHatsService> mock_hats_service_;
};

class NewTabPageHandlerThemeTest
    : public NewTabPageHandlerTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  NewTabPageHandlerThemeTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (RemoveScrim()) {
      enabled_features.push_back(ntp_features::kNtpRemoveScrim);
    } else {
      disabled_features.push_back(ntp_features::kNtpRemoveScrim);
    }

    if (CustomizeChromeSidePanel()) {
      enabled_features.push_back(ntp_features::kCustomizeChromeSidePanel);
    } else {
      disabled_features.push_back(ntp_features::kCustomizeChromeSidePanel);
    }

    feature_list_.InitWithFeatures(std::move(enabled_features),
                                   std::move(disabled_features));
  }

  bool RemoveScrim() const { return std::get<0>(GetParam()); }
  bool CustomizeChromeSidePanel() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(NewTabPageHandlerThemeTest, SetTheme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::optional<CustomBackground>()));
  mock_color_provider_source_.SetColor(kColorNewTabPageBackground,
                                       SkColorSetRGB(0, 0, 1));
  mock_color_provider_source_.SetColor(kColorNewTabPageText,
                                       SkColorSetRGB(0, 0, 2));
  mock_color_provider_source_.SetColor(kColorNewTabPageTextUnthemed,
                                       SkColorSetRGB(0, 0, 3));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_service_, UsingAutogeneratedTheme())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE))
      .WillByDefault(testing::Return(1));
  mock_color_provider_source_.SetColor(kColorNewTabPageLogo,
                                       SkColorSetRGB(0, 0, 4));
  mock_color_provider_source_.SetColor(kColorNewTabPageLogoUnthemedLight,
                                       SkColorSetRGB(0, 0, 5));
  ON_CALL(mock_theme_service_, GetThemeID())
      .WillByDefault(testing::Return("bar"));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING))
      .WillByDefault(testing::Return(ThemeProperties::REPEAT_X));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT))
      .WillByDefault(testing::Return(ThemeProperties::ALIGN_TOP));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_ATTRIBUTION))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackground, SkColorSetRGB(0, 0, 6));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundThemed, SkColorSetRGB(0, 0, 7));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundUnthemed,
      SkColorSetRGB(0, 0, 8));

  theme_service_observer_->OnThemeChanged();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), theme->background_color);
  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->is_dark);
  EXPECT_FALSE(theme->daily_refresh_enabled);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?bar",
            theme->background_image->url);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?bar",
            theme->background_image->url_2x);
  EXPECT_EQ("chrome://theme/IDR_THEME_NTP_ATTRIBUTION?bar",
            theme->background_image->attribution_url);
  EXPECT_EQ("initial", theme->background_image->size);
  EXPECT_EQ("repeat", theme->background_image->repeat_x);
  EXPECT_EQ("no-repeat", theme->background_image->repeat_y);
  EXPECT_EQ("center", theme->background_image->position_x);
  EXPECT_EQ("top", theme->background_image->position_y);
  if (!RemoveScrim()) {
    EXPECT_EQ(SkColorSetRGB(0, 0, 2), theme->text_color);
    EXPECT_EQ(SkColorSetRGB(0, 0, 4), theme->logo_color);
    EXPECT_EQ(RemoveScrim(),
              theme->background_image->scrim_display.has_value());
  } else {
    EXPECT_EQ(SkColorSetRGB(0, 0, 3), theme->text_color);
    EXPECT_EQ(SkColorSetRGB(0, 0, 5), theme->logo_color);
    EXPECT_TRUE(theme->background_image->scrim_display.has_value());
    EXPECT_EQ("none", theme->background_image->scrim_display.value());
  }
  EXPECT_FALSE(theme->background_image_attribution_1.has_value());
  EXPECT_FALSE(theme->background_image_attribution_2.has_value());
  EXPECT_FALSE(theme->background_image_attribution_url.has_value());
  EXPECT_FALSE(theme->background_image_collection_id.has_value());
  ASSERT_TRUE(theme->most_visited);
  EXPECT_EQ(SkColorSetRGB(0, 0, 8), theme->most_visited->background_color);
  if (RemoveScrim()) {
    EXPECT_FALSE(theme->most_visited->use_title_pill);
  } else {
    EXPECT_TRUE(theme->most_visited->use_title_pill);
  }
  EXPECT_TRUE(theme->most_visited->use_white_tile_icon);
  EXPECT_EQ(false, theme->most_visited->is_dark);
}

TEST_P(NewTabPageHandlerThemeTest, SetCustomBackground) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.custom_background_attribution_line_2 = "bar line";
  custom_background.custom_background_attribution_action_url =
      GURL("https://foo.com/action");
  custom_background.collection_id = "baz collection";
  custom_background.daily_refresh_enabled = false;
  custom_background.is_uploaded_image = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  mock_color_provider_source_.SetColor(kColorNewTabPageBackground,
                                       SkColorSetRGB(0, 0, 1));
  mock_color_provider_source_.SetColor(kColorNewTabPageTextUnthemed,
                                       SkColorSetRGB(0, 0, 2));
  mock_color_provider_source_.SetColor(kColorNewTabPageLogoUnthemedLight,
                                       SkColorSetRGB(0, 0, 3));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackground, SkColorSetRGB(0, 0, 4));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundUnthemed,
      SkColorSetRGB(0, 0, 5));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  if (CustomizeChromeSidePanel()) {
    EXPECT_FALSE(theme->is_custom_background);
    EXPECT_FALSE(theme->background_image_attribution_1.has_value());
    EXPECT_FALSE(theme->background_image_attribution_2.has_value());
    EXPECT_FALSE(theme->background_image_attribution_url.has_value());
    EXPECT_FALSE(theme->background_image_collection_id.has_value());
  } else {
    ASSERT_TRUE(theme);
    EXPECT_TRUE(theme->is_custom_background);
    EXPECT_EQ(SkColorSetRGB(0, 0, 1), theme->background_color);
    EXPECT_EQ(SkColorSetRGB(0, 0, 2), theme->text_color);
    EXPECT_EQ(SkColorSetRGB(0, 0, 3), theme->logo_color);
    EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
    EXPECT_EQ("foo line", theme->background_image_attribution_1);
    EXPECT_EQ("bar line", theme->background_image_attribution_2);
    EXPECT_EQ("https://foo.com/action",
              theme->background_image_attribution_url);
    EXPECT_FALSE(theme->daily_refresh_enabled);
    EXPECT_EQ("baz collection", theme->background_image_collection_id);
    EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::
                  kFirstPartyThemeWithoutDailyRefresh,
              theme->background_image->image_source);
  }
  if (RemoveScrim()) {
    EXPECT_TRUE(theme->background_image->scrim_display.has_value());
    EXPECT_EQ("none", theme->background_image->scrim_display.value());
  } else {
    EXPECT_FALSE(theme->background_image->scrim_display.has_value());
  }

  EXPECT_EQ(SkColorSetRGB(0, 0, 4), theme->most_visited->background_color);
}

TEST_P(NewTabPageHandlerThemeTest, SetDailyRefresh) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  CustomBackground custom_background;
  custom_background.daily_refresh_enabled = true;
  custom_background.is_uploaded_image = false;
  custom_background.collection_id = "baz collection";
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  if (CustomizeChromeSidePanel()) {
    EXPECT_FALSE(theme->is_custom_background);
    EXPECT_FALSE(theme->background_image_collection_id.has_value());
  } else {
    ASSERT_TRUE(theme);
    EXPECT_TRUE(theme->is_custom_background);
    EXPECT_TRUE(theme->daily_refresh_enabled);
    EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::
                  kFirstPartyThemeWithDailyRefresh,
              theme->background_image->image_source);
    EXPECT_EQ("baz collection", theme->background_image_collection_id);
  }
}

TEST_P(NewTabPageHandlerThemeTest, SetUploadedImage) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.is_uploaded_image = true;
  custom_background.daily_refresh_enabled = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::make_optional(custom_background)));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::kUploadedImage,
            theme->background_image->image_source);
}

TEST_P(NewTabPageHandlerThemeTest, SetThirdPartyTheme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.collection_id = "baz collection";
  custom_background.daily_refresh_enabled = false;
  custom_background.is_uploaded_image = false;

  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_service_, UsingExtensionTheme())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_service_, GetThemeID())
      .WillByDefault(testing::Return("foo"));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);

  if (CustomizeChromeSidePanel()) {
    EXPECT_FALSE(theme->is_custom_background);
    EXPECT_FALSE(theme->background_image_collection_id.has_value());
    EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::kThirdPartyTheme,
              theme->background_image->image_source);
  } else {
    EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
    EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::
                  kFirstPartyThemeWithoutDailyRefresh,
              theme->background_image->image_source);
    EXPECT_EQ("baz collection", theme->background_image_collection_id);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageHandlerThemeTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(NewTabPageHandlerTest, Histograms) {
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 0);

  handler_->OnDismissModule("shopping_tasks");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleDismissedHistogram) +
          ".shopping_tasks",
      1);

  handler_->OnRestoreModule("kaleidoscope");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleRestoredHistogram) +
          ".kaleidoscope",
      1);

  // NewTabPage.Modules.FreOptIn and NewTabPage.Modules.FreOptOut log how many
  // times the FRE is shown, so we increment the shown count to make sure the
  // histogram is logging correctly
  handler_->IncrementModulesShownCount();
  handler_->LogModulesFreOptInStatus(
      new_tab_page::mojom::OptInStatus::kExplicitOptIn);
  histogram_tester_.ExpectTotalCount("NewTabPage.Modules.FreExplicitOptIn", 1);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.FreExplicitOptIn", 1));

  handler_->IncrementModulesShownCount();
  handler_->LogModulesFreOptInStatus(new_tab_page::mojom::OptInStatus::kOptOut);
  histogram_tester_.ExpectTotalCount("NewTabPage.Modules.FreOptOut", 1);
  ASSERT_EQ(
      1, histogram_tester_.GetBucketCount("NewTabPage.Modules.FreOptOut", 2));
}

TEST_F(NewTabPageHandlerTest, GetAnimatedDoodle) {
  search_provider_logos::EncodedLogo logo;
  logo.encoded_image =
      base::MakeRefCounted<base::RefCountedString>(std::string("light image"));
  logo.dark_encoded_image =
      base::MakeRefCounted<base::RefCountedString>(std::string("dark image"));
  logo.metadata.type = search_provider_logos::LogoType::ANIMATED;
  logo.metadata.on_click_url = GURL("https://doodle.com/on_click_url");
  logo.metadata.alt_text = "alt text";
  logo.metadata.mime_type = "light_mime_type";
  logo.metadata.dark_mime_type = "dark_mime_type";
  logo.metadata.dark_background_color = "#000001";
  logo.metadata.animated_url = GURL("https://doodle.com/light_animation");
  logo.metadata.dark_animated_url = GURL("https://doodle.com/dark_animation");
  logo.metadata.cta_log_url = GURL("https://doodle.com/light_cta_log_url");
  logo.metadata.dark_cta_log_url = GURL("https://doodle.com/dark_cta_log_url");
  logo.metadata.log_url = GURL("https://doodle.com/light_log_url");
  logo.metadata.dark_log_url = GURL("https://doodle.com/dark_log_url");
  logo.metadata.short_link = GURL("https://doodle.com/short_link");
  logo.metadata.width_px = 1;
  logo.metadata.height_px = 2;
  logo.metadata.dark_width_px = 3;
  logo.metadata.dark_height_px = 4;
  logo.metadata.share_button_x = 5;
  logo.metadata.dark_share_button_x = 6;
  logo.metadata.share_button_y = 7;
  logo.metadata.dark_share_button_y = 8;
  logo.metadata.share_button_opacity = 0.5;
  logo.metadata.dark_share_button_opacity = 0.7;
  logo.metadata.share_button_icon = "light share button";
  logo.metadata.dark_share_button_icon = "dark share button";
  logo.metadata.share_button_bg = "#000100";
  logo.metadata.dark_share_button_bg = "#010000";

  auto doodle = GetDoodle(logo);

  ASSERT_TRUE(doodle);
  ASSERT_TRUE(doodle->image);
  ASSERT_FALSE(doodle->interactive);
  EXPECT_EQ("data:light_mime_type;base64,bGlnaHQgaW1hZ2U=",
            doodle->image->light->image_url);
  EXPECT_EQ("https://doodle.com/light_animation",
            doodle->image->light->animation_url);
  EXPECT_EQ(1u, doodle->image->light->width);
  EXPECT_EQ(2u, doodle->image->light->height);
  EXPECT_EQ(SK_ColorWHITE, doodle->image->light->background_color);
  EXPECT_EQ(5, doodle->image->light->share_button->x);
  EXPECT_EQ(7, doodle->image->light->share_button->y);
  EXPECT_EQ(SkColorSetARGB(127, 0, 1, 0),
            doodle->image->light->share_button->background_color);
  EXPECT_EQ("data:image/png;base64,light share button",
            doodle->image->light->share_button->icon_url);
  EXPECT_EQ("https://doodle.com/light_cta_log_url",
            doodle->image->light->image_impression_log_url);
  EXPECT_EQ("https://doodle.com/light_log_url",
            doodle->image->light->animation_impression_log_url);
  EXPECT_EQ("data:dark_mime_type;base64,ZGFyayBpbWFnZQ==",
            doodle->image->dark->image_url);
  EXPECT_EQ("https://doodle.com/dark_animation",
            doodle->image->dark->animation_url);
  EXPECT_EQ(3u, doodle->image->dark->width);
  EXPECT_EQ(4u, doodle->image->dark->height);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), doodle->image->dark->background_color);
  EXPECT_EQ(6, doodle->image->dark->share_button->x);
  EXPECT_EQ(8, doodle->image->dark->share_button->y);
  EXPECT_EQ(SkColorSetARGB(178, 1, 0, 0),
            doodle->image->dark->share_button->background_color);
  EXPECT_EQ("data:image/png;base64,dark share button",
            doodle->image->dark->share_button->icon_url);
  EXPECT_EQ("https://doodle.com/dark_cta_log_url",
            doodle->image->dark->image_impression_log_url);
  EXPECT_EQ("https://doodle.com/dark_log_url",
            doodle->image->dark->animation_impression_log_url);
  EXPECT_EQ("https://doodle.com/on_click_url", doodle->image->on_click_url);
  EXPECT_EQ("https://doodle.com/short_link", doodle->image->share_url);
  EXPECT_EQ("alt text", doodle->description);
}

TEST_F(NewTabPageHandlerTest, GetInteractiveDoodle) {
  search_provider_logos::EncodedLogo logo;
  logo.metadata.type = search_provider_logos::LogoType::INTERACTIVE;
  logo.metadata.full_page_url = GURL("https://doodle.com/full_page_url");
  logo.metadata.iframe_width_px = 1;
  logo.metadata.iframe_height_px = 2;
  logo.metadata.alt_text = "alt text";

  auto doodle = GetDoodle(logo);

  EXPECT_EQ("https://doodle.com/full_page_url", doodle->interactive->url);
  EXPECT_EQ(1u, doodle->interactive->width);
  EXPECT_EQ(2u, doodle->interactive->height);
  EXPECT_EQ("alt text", doodle->description);
}

TEST_F(NewTabPageHandlerTest, UpdatePromoData) {
  PromoData promo_data;
  promo_data.middle_slot_json = R"({
    "part": [{
      "image": {
        "image_url": "https://image.com/image",
        "target": "https://image.com/target"
      }
    }, {
      "link": {
        "url": "https://link.com",
        "text": "bar",
        "color": "red"
      }
    }, {
      "text": {
        "text": "blub",
        "color": "green"
      }
    }]
  })";
  promo_data.promo_log_url = GURL("https://foo.com");
  promo_data.promo_id = "foo";
  auto promo_data_optional = absl::make_optional(promo_data);
  ON_CALL(*mock_promo_service_, promo_data())
      .WillByDefault(testing::ReturnRef(promo_data_optional));
  EXPECT_CALL(*mock_promo_service_, Refresh).Times(1);

  new_tab_page::mojom::PromoPtr promo;
  EXPECT_CALL(mock_page_, SetPromo)
      .Times(1)
      .WillOnce(testing::Invoke([&promo](new_tab_page::mojom::PromoPtr arg) {
        promo = std::move(arg);
      }));
  handler_->UpdatePromoData();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(promo);
  EXPECT_EQ("foo", promo->id);
  EXPECT_EQ("https://foo.com/", promo->log_url);
  ASSERT_EQ(3lu, promo->middle_slot_parts.size());
  ASSERT_TRUE(promo->middle_slot_parts[0]->is_image());
  const auto& image = promo->middle_slot_parts[0]->get_image();
  EXPECT_EQ("https://image.com/image", image->image_url);
  EXPECT_EQ("https://image.com/target", image->target);
  ASSERT_TRUE(promo->middle_slot_parts[1]->is_link());
  const auto& link = promo->middle_slot_parts[1]->get_link();
  EXPECT_EQ("bar", link->text);
  EXPECT_EQ("https://link.com/", link->url);
  ASSERT_TRUE(promo->middle_slot_parts[2]->is_text());
  const auto& text = promo->middle_slot_parts[2]->get_text();
  EXPECT_EQ("blub", text->text);
}

TEST_F(NewTabPageHandlerTest, OnDoodleImageClicked) {
  handler_->OnDoodleImageClicked(
      /*type=*/new_tab_page::mojom::DoodleImageType::kCta,
      /*log_url=*/GURL("https://doodle.com/log"));

  histogram_tester_.ExpectTotalCount("NewTabPage.LogoClick", 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://doodle.com/log", ""));
}

TEST_F(NewTabPageHandlerTest, OnDoodleImageRendered) {
  base::MockCallback<NewTabPageHandler::OnDoodleImageRenderedCallback> callback;
  absl::optional<std::string> image_click_params;
  absl::optional<GURL> interaction_log_url;
  absl::optional<std::string> shared_id;
  EXPECT_CALL(callback, Run(_, _, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&image_click_params),
                      SaveArg<1>(&interaction_log_url),
                      SaveArg<2>(&shared_id)));

  handler_->OnDoodleImageRendered(
      /*type=*/new_tab_page::mojom::DoodleImageType::kStatic,
      /*time=*/0,
      /*log_url=*/GURL("https://doodle.com/log"), callback.Get());

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://doodle.com/log", R"()]}'
  {
    "ddllog": {
      "target_url_params": "foo params",
      "interaction_log_url": "/bar_log",
      "encoded_ei": "baz ei"
    }
  })"));
  EXPECT_THAT(image_click_params, Optional(std::string("foo params")));
  EXPECT_THAT(interaction_log_url,
              Optional(GURL("https://www.google.com/bar_log")));
  EXPECT_THAT(shared_id, Optional(std::string("baz ei")));
  histogram_tester_.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histogram_tester_.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histogram_tester_.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

TEST_F(NewTabPageHandlerTest, OnDoodleShared) {
  handler_->OnDoodleShared(new_tab_page::mojom::DoodleShareChannel::kEmail,
                           "food_id", "bar_id");

  EXPECT_TRUE(test_url_loader_factory_.IsPending(
      "https://www.google.com/"
      "gen_204?atype=i&ct=doodle&ntp=2&cad=sh,5,ct:food_id&ei=bar_id"));
}

TEST_F(NewTabPageHandlerTest, GetModulesIdNames) {
  std::vector<new_tab_page::mojom::ModuleIdNamePtr> modules_details;
  base::MockCallback<NewTabPageHandler::GetModulesIdNamesCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&modules_details](
              std::vector<new_tab_page::mojom::ModuleIdNamePtr> arg) {
            modules_details = std::move(arg);
          }));
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpRecipeTasksModule},
      /*disabled_features=*/{});
  handler_->GetModulesIdNames(callback.Get());
  EXPECT_EQ(modules_details.size(), 1u);
  EXPECT_EQ(modules_details.front()->id, "recipe_tasks");
}

TEST_F(NewTabPageHandlerTest, GetModulesOrder) {
  std::vector<std::string> module_ids;
  base::MockCallback<NewTabPageHandler::GetModulesOrderCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(SaveArg<0>(&module_ids));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpModulesOrder,
        {{ntp_features::kNtpModulesOrderParam, "bar,baz"}}},
       {ntp_features::kNtpModulesDragAndDrop, {}}},
      {});
  base::Value::List module_ids_value;
  module_ids_value.Append("foo");
  module_ids_value.Append("bar");
  profile_->GetPrefs()->SetList(prefs::kNtpModulesOrder,
                                std::move(module_ids_value));

  handler_->GetModulesOrder(callback.Get());
  EXPECT_THAT(module_ids, ElementsAre("foo", "bar", "baz"));
}

TEST_F(NewTabPageHandlerTest, SurveyLaunchedEligibleModulesCriteria) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {features::kHappinessTrackingSurveysForDesktopNtpModules,
           {{ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam,
             "recipe_tasks,drive"}}},
      },
      {});

  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _))
      .Times(1);
  const std::vector<std::string> module_ids = {"recipe_tasks", "cart"};
  handler_->OnModulesLoadedWithData(module_ids);
}

TEST_F(NewTabPageHandlerTest, SurveyLaunchSkippedEligibleModulesCriteria) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {features::kHappinessTrackingSurveysForDesktopNtpModules,
           {{ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam,
             "drive"}}},
      },
      {});

  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _))
      .Times(0);
  const std::vector<std::string> module_ids = {"recipe_tasks"};
  handler_->OnModulesLoadedWithData(module_ids);
}

TEST_F(NewTabPageHandlerTest, UpdateNtpModulesFreVisibility) {
  bool expected_visibility = true;
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesFreVisible,
                                   expected_visibility);

  EXPECT_EQ(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible),
            expected_visibility);

  expected_visibility = false;
  EXPECT_CALL(mock_page_, SetModulesFreVisibility)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](bool arg) { EXPECT_EQ(expected_visibility, arg); }));

  handler_->SetModulesFreVisible(expected_visibility);

  EXPECT_EQ(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible),
            expected_visibility);

  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, IncrementModulesShownCount) {
  EXPECT_EQ(profile_->GetPrefs()->GetInteger(prefs::kNtpModulesShownCount), 0);
  EXPECT_EQ(profile_->GetPrefs()->GetTime(prefs::kNtpModulesFirstShownTime),
            base::Time());

  handler_->IncrementModulesShownCount();

  EXPECT_EQ(profile_->GetPrefs()->GetInteger(prefs::kNtpModulesShownCount), 1);
  EXPECT_NE(profile_->GetPrefs()->GetTime(prefs::kNtpModulesFirstShownTime),
            base::Time());

  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest,
       UpdateModulesFreVisibilityUsingModulesShownCount) {
  handler_->SetModulesFreVisible(true);
  profile_->GetPrefs()->SetInteger(prefs::kNtpModulesShownCount, 7);

  handler_->UpdateModulesFreVisibility();

  EXPECT_EQ(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible),
            true);

  profile_->GetPrefs()->SetInteger(prefs::kNtpModulesShownCount, 8);

  handler_->UpdateModulesFreVisibility();

  EXPECT_EQ(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible),
            false);
  histogram_tester_.ExpectTotalCount("NewTabPage.Modules.FreImplicitOptIn", 1);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Modules.FreImplicitOptIn", true));

  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest,
       UpdateModulesFreVisibilityUsingModulesFirstShownTime) {
  handler_->SetModulesFreVisible(true);
  profile_->GetPrefs()->SetTime(prefs::kNtpModulesFirstShownTime,
                                base::Time::Now());

  handler_->UpdateModulesFreVisibility();

  EXPECT_EQ(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible),
            true);

  profile_->GetPrefs()->SetTime(prefs::kNtpModulesFirstShownTime,
                                base::Time::Now() - base::Days(2));

  handler_->UpdateModulesFreVisibility();

  EXPECT_EQ(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible),
            false);

  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, SetModuleDisabledTriggersPageCall) {
  handler_->SetModuleDisabled("drive", true);
  EXPECT_CALL(mock_page_, SetDisabledModules).Times(1);
  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, ModulesVisiblePrefChangeTriggersPageCall) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, true);
  EXPECT_CALL(mock_page_, SetDisabledModules).Times(1);
  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, SetCustomizeChromeSidePanelVisible) {
  bool visible;
  CustomizeChromeSection section;
  EXPECT_CALL(*mock_customize_chrome_tab_helper_,
              SetCustomizeChromeSidePanelVisible)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&visible),
                               testing::SaveArg<1>(&section)));
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              RecordCustomizeChromeFeatureUsage)
      .Times(1);
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              CloseCustomizeChromeFeaturePromo)
      .Times(1);

  handler_->SetCustomizeChromeSidePanelVisible(
      /*visible=*/true,
      new_tab_page::mojom::CustomizeChromeSection::kAppearance);

  EXPECT_TRUE(visible);
  EXPECT_EQ(CustomizeChromeSection::kAppearance, section);
}

TEST_F(NewTabPageHandlerTest, SetCustomizeChromeSidePanelInvisible) {
  bool visible;
  CustomizeChromeSection section;
  EXPECT_CALL(*mock_customize_chrome_tab_helper_,
              SetCustomizeChromeSidePanelVisible)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&visible),
                               testing::SaveArg<1>(&section)));
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              RecordCustomizeChromeFeatureUsage)
      .Times(0);
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              CloseCustomizeChromeFeaturePromo)
      .Times(0);

  handler_->SetCustomizeChromeSidePanelVisible(
      /*visible=*/false, new_tab_page::mojom::CustomizeChromeSection::kModules);

  EXPECT_FALSE(visible);
  EXPECT_EQ(CustomizeChromeSection::kModules, section);
}

TEST_F(NewTabPageHandlerTest, IncrementCustomizeChromeButtonOpenCount) {
  EXPECT_EQ(profile_->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            0);

  handler_->IncrementCustomizeChromeButtonOpenCount();

  EXPECT_EQ(profile_->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            1);

  handler_->IncrementCustomizeChromeButtonOpenCount();

  EXPECT_EQ(profile_->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            2);

  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, MaybeShowCustomizeChromeFeaturePromo) {
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              IsSigninModalDialogOpen)
      .WillRepeatedly(testing::Return(false));
  EXPECT_EQ(profile_->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            0);
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              MaybeShowCustomizeChromeFeaturePromo)
      .Times(1);

  handler_->MaybeShowCustomizeChromeFeaturePromo();

  handler_->IncrementCustomizeChromeButtonOpenCount();
  EXPECT_EQ(profile_->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            1);
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              MaybeShowCustomizeChromeFeaturePromo)
      .Times(0);

  handler_->MaybeShowCustomizeChromeFeaturePromo();

  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest,
       DontShowCustomizeChromeFeaturePromoWhenModalDialogIsOpen) {
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              IsSigninModalDialogOpen)
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(profile_->GetPrefs()->GetInteger(
                prefs::kNtpCustomizeChromeButtonOpenCount),
            0);
  EXPECT_CALL(*mock_customize_chrome_feature_promo_helper_,
              MaybeShowCustomizeChromeFeaturePromo)
      .Times(0);

  handler_->MaybeShowCustomizeChromeFeaturePromo();

  mock_page_.FlushForTesting();
}
