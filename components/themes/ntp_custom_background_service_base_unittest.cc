// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/themes/ntp_background_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {
const char kTestDictPref[] = "dict_pref";
const char kTestLocalPref[] = "local_pref";
const char kTestLocale[] = "en-US";
const char kTestBackgroundUrlKey[] = "background_url";
const char kTestBackgroundUrl[] = "https://example.com/image.jpg";
const char kValidBackgroundUrl[] = "https://www.foo.com/";
const char kValidActionUrl[] = "https://action.com/";
const char kAttributionLine1[] = "line1";
const char kAttributionLine2[] = "line2";
const char kCollectionId[] = "collection";
const char kResumeToken[] = "resume_token";
const char kOtherBackgroundUrl[] = "https://other.com/image.png";
}  // namespace

class MockNtpCustomBackgroundServiceObserver
    : public NtpCustomBackgroundServiceObserver {
 public:
  MOCK_METHOD(void, OnCustomBackgroundImageUpdated, (), (override));
  MOCK_METHOD(void, OnNtpCustomBackgroundServiceShuttingDown, (), (override));
};

class TestNtpCustomBackgroundServiceBase
    : public NtpCustomBackgroundServiceBase {
 public:
  TestNtpCustomBackgroundServiceBase(PrefService* pref_service,
                                     NtpBackgroundService* background_service)
      : NtpCustomBackgroundServiceBase(pref_service,
                                       background_service,
                                       kTestDictPref,
                                       kTestLocalPref) {}

  using NtpCustomBackgroundServiceBase::IsCustomBackgroundPrefValid;
  using NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds;

  void SelectLocalBackgroundImage(const base::FilePath& path) override {}
};

class NtpCustomBackgroundServiceBaseTest : public testing::Test {
 public:
  NtpCustomBackgroundServiceBaseTest() = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(kTestDictPref);
    pref_service_->registry()->RegisterBooleanPref(kTestLocalPref, false);

    application_locale_storage_ = std::make_unique<ApplicationLocaleStorage>();
    application_locale_storage_->Set(kTestLocale);

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    background_service_ = std::make_unique<NtpBackgroundService>(
        application_locale_storage_.get(), test_shared_loader_factory_);

    service_ = std::make_unique<TestNtpCustomBackgroundServiceBase>(
        pref_service_.get(), background_service_.get());
  }

  void TearDown() override {
    service_.reset();
    background_service_.reset();
    test_shared_loader_factory_.reset();
    application_locale_storage_.reset();
    pref_service_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;
  std::unique_ptr<NtpBackgroundService> background_service_;
  std::unique_ptr<TestNtpCustomBackgroundServiceBase> service_;
};

TEST_F(NtpCustomBackgroundServiceBaseTest, Initialization) {
  EXPECT_NE(service_, nullptr);
}

TEST_F(NtpCustomBackgroundServiceBaseTest, Observers) {
  testing::StrictMock<MockNtpCustomBackgroundServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnCustomBackgroundImageUpdated());
  service_->NotifyAboutBackgrounds();

  service_->RemoveObserver(&observer);
  // StrictMock will fail if the observer is still notified.
  service_->NotifyAboutBackgrounds();
}

TEST_F(NtpCustomBackgroundServiceBaseTest, ResetCustomBackgroundInfo) {
  pref_service_->SetBoolean(kTestLocalPref, true);
  base::DictValue dict;
  dict.Set(kTestBackgroundUrlKey, base::Value(kTestBackgroundUrl));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  service_->ResetCustomBackgroundInfo();

  EXPECT_FALSE(pref_service_->GetBoolean(kTestLocalPref));
  EXPECT_TRUE(pref_service_->GetDict(kTestDictPref).empty());
}

TEST_F(NtpCustomBackgroundServiceBaseTest, IsCustomBackgroundPrefValid) {
  EXPECT_FALSE(service_->IsCustomBackgroundPrefValid());

  base::DictValue dict;
  dict.Set(kTestBackgroundUrlKey, base::Value(kTestBackgroundUrl));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  EXPECT_TRUE(service_->IsCustomBackgroundPrefValid());
}

TEST_F(NtpCustomBackgroundServiceBaseTest, SetCustomBackgroundInfo) {
  const GURL kUrl(kValidBackgroundUrl);
  background_service_->AddValidBackdropUrlForTesting(kUrl);

  service_->SetCustomBackgroundInfo(kUrl, GURL(), kAttributionLine1, kAttributionLine2,
                                    GURL(kValidActionUrl), kCollectionId);

  const base::DictValue& dict = pref_service_->GetDict(kTestDictPref);
  EXPECT_FALSE(dict.empty());

  const std::string* url = dict.FindString(kNtpCustomBackgroundURL);
  ASSERT_TRUE(url);
  EXPECT_EQ(*url, kValidBackgroundUrl);

  const std::string* collection_id = dict.FindString(kNtpCustomBackgroundCollectionId);
  ASSERT_TRUE(collection_id);
  EXPECT_EQ(*collection_id, kCollectionId);
}

TEST_F(NtpCustomBackgroundServiceBaseTest, UpdateBackgroundFromSync) {
  pref_service_->SetBoolean(kTestLocalPref, true);
  testing::StrictMock<MockNtpCustomBackgroundServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnCustomBackgroundImageUpdated());
  service_->UpdateBackgroundFromSync();

  EXPECT_FALSE(pref_service_->GetBoolean(kTestLocalPref));

  service_->RemoveObserver(&observer);
}

TEST_F(NtpCustomBackgroundServiceBaseTest, RefreshBackgroundIfNeeded) {
  base::DictValue dict;
  dict.Set(kTestBackgroundUrlKey, base::Value(kTestBackgroundUrl));
  dict.Set(kNtpCustomBackgroundCollectionId, base::Value(kCollectionId));
  dict.Set(kNtpCustomBackgroundResumeToken, base::Value(kResumeToken));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  // Will hit the TestURLLoaderFactory via NtpBackgroundService and queue a
  // request.
  service_->RefreshBackgroundIfNeeded();

  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
}

TEST_F(NtpCustomBackgroundServiceBaseTest, GetCustomBackground) {
  base::DictValue dict;
  dict.Set(kNtpCustomBackgroundURL, base::Value(kTestBackgroundUrl));
  dict.Set(kNtpCustomBackgroundCollectionId, base::Value(kCollectionId));
  dict.Set(kNtpCustomBackgroundAttributionLine1,
           base::Value(kAttributionLine1));
  dict.Set(kNtpCustomBackgroundAttributionLine2,
           base::Value(kAttributionLine2));
  dict.Set(kNtpCustomBackgroundAttributionActionURL,
           base::Value(kValidActionUrl));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  std::optional<CustomBackground> background = service_->GetCustomBackground();
  ASSERT_TRUE(background.has_value());
  EXPECT_EQ(background->custom_background_url, GURL(kTestBackgroundUrl));
  EXPECT_EQ(background->collection_id, kCollectionId);
  EXPECT_EQ(background->custom_background_attribution_line_1,
            kAttributionLine1);
  EXPECT_EQ(background->custom_background_attribution_line_2,
            kAttributionLine2);
  EXPECT_EQ(background->custom_background_attribution_action_url,
            GURL(kValidActionUrl));
  EXPECT_FALSE(background->is_uploaded_image);

  // If local pref is true, it returns nullopt
  pref_service_->SetBoolean(kTestLocalPref, true);
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
}

TEST_F(NtpCustomBackgroundServiceBaseTest, OnNextCollectionImageAvailable) {
  CollectionImage image;
  image.collection_id = kCollectionId;
  image.image_url = GURL(kTestBackgroundUrl);
  image.attribution.push_back(kAttributionLine1);
  image.attribution.push_back(kAttributionLine2);
  image.attribution_action_url = GURL(kValidActionUrl);

  background_service_->SetNextCollectionImageForTesting(image);

  service_->OnNextCollectionImageAvailable();

  const base::DictValue& dict = pref_service_->GetDict(kTestDictPref);
  EXPECT_FALSE(dict.empty());

  const std::string* url = dict.FindString(kNtpCustomBackgroundURL);
  ASSERT_TRUE(url);
  EXPECT_EQ(*url, kTestBackgroundUrl);
}

TEST_F(NtpCustomBackgroundServiceBaseTest, OnNtpBackgroundServiceShuttingDown) {
  service_->OnNtpBackgroundServiceShuttingDown();
  // We just ensure it doesn't crash. State changes internally resetting
  // observation.
}

TEST_F(NtpCustomBackgroundServiceBaseTest,
       UpdateCustomBackgroundPrefsWithColor_UpdatesColor) {
  const GURL kUrl(kValidBackgroundUrl);
  background_service_->AddValidBackdropUrlForTesting(kUrl);

  service_->SetCustomBackgroundInfo(kUrl, GURL(), kAttributionLine1,
                                    kAttributionLine2, GURL(kValidActionUrl),
                                    kCollectionId);

  constexpr SkColor kTestColor = SK_ColorRED;
  EXPECT_TRUE(service_->UpdateCustomBackgroundPrefsWithColor(kUrl, kTestColor));

  const base::DictValue& dict = pref_service_->GetDict(kTestDictPref);
  EXPECT_FALSE(dict.empty());
  std::optional<int> main_color = dict.FindInt(kNtpCustomBackgroundMainColor);
  ASSERT_TRUE(main_color.has_value());
  EXPECT_EQ(static_cast<int>(kTestColor), *main_color);
}

TEST_F(NtpCustomBackgroundServiceBaseTest,
       UpdateCustomBackgroundPrefsWithColor_MismatchedUrl_ReturnsFalse) {
  const GURL kUrl(kValidBackgroundUrl);
  background_service_->AddValidBackdropUrlForTesting(kUrl);

  service_->SetCustomBackgroundInfo(kUrl, GURL(), kAttributionLine1,
                                    kAttributionLine2, GURL(kValidActionUrl),
                                    kCollectionId);

  constexpr SkColor kTestColor = SK_ColorRED;
  EXPECT_FALSE(service_->UpdateCustomBackgroundPrefsWithColor(
      GURL(kOtherBackgroundUrl), kTestColor));

  const base::DictValue& dict = pref_service_->GetDict(kTestDictPref);
  EXPECT_FALSE(dict.FindInt(kNtpCustomBackgroundMainColor).has_value());
}

TEST_F(NtpCustomBackgroundServiceBaseTest,
       UpdateCustomBackgroundPrefsWithColor_NoBackgroundSet_ReturnsFalse) {
  constexpr SkColor kTestColor = SK_ColorBLUE;
  EXPECT_FALSE(service_->UpdateCustomBackgroundPrefsWithColor(
      GURL(kValidBackgroundUrl), kTestColor));

  const base::DictValue& dict = pref_service_->GetDict(kTestDictPref);
  EXPECT_TRUE(dict.empty());
}
