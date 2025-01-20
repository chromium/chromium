// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <memory>

#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/regional_capabilities/android/test_utils_jni_headers/RegionalCapabilitiesServiceTestUtil_jni.h"
#endif

namespace regional_capabilities {

namespace {

#if BUILDFLAG(IS_ANDROID)
class TestSupportAndroid {
 public:
  TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_ref =
        Java_RegionalCapabilitiesServiceTestUtil_Constructor(env);
    java_test_util_ref_.Reset(env, java_ref.obj());
  }

  ~TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_RegionalCapabilitiesServiceTestUtil_destroy(env, java_test_util_ref_);
  }

  void ReturnDeviceCountry(const std::string& device_country) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_RegionalCapabilitiesServiceTestUtil_returnDeviceCountry(
        env, java_test_util_ref_,
        base::android::ConvertUTF8ToJavaString(env, device_country));
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_test_util_ref_;
};
#endif

const int kBelgiumCountryId = country_codes::CountryCharsToCountryID('B', 'E');

}  // namespace

class RegionalCapabilitiesServiceTest : public ::testing::Test {
 public:
  RegionalCapabilitiesServiceTest() {
    country_codes::RegisterProfilePrefs(pref_service_.registry());

    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");
  }

  ~RegionalCapabilitiesServiceTest() override = default;

  void InitService(
      int variation_country_id = country_codes::kCountryIDUnknown) {
    CHECK(!regional_capabilities_service_);
    std::unique_ptr<RegionalCapabilitiesService::Client> client =
#if BUILDFLAG(IS_ANDROID)
        // Use a real C++ client to test the JNI integration code, faking is
        // done via `TestSupportAndroid`.
        std::make_unique<RegionalCapabilitiesService::Client>();
#else
        std::make_unique<FakeRegionalCapabilitiesServiceClient>(
            variation_country_id);
#endif

    regional_capabilities_service_ =
        std::make_unique<RegionalCapabilitiesService>(pref_service_,
                                                      std::move(client));
  }

  int GetCountryId() { return regional_capabilities_service().GetCountryId(); }

  RegionalCapabilitiesService& regional_capabilities_service() {
    if (!regional_capabilities_service_) {
      InitService();
    }

    return CHECK_DEREF(regional_capabilities_service_.get());
  }

  sync_preferences::TestingPrefServiceSyncable& pref_service() {
    return pref_service_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<RegionalCapabilitiesService> regional_capabilities_service_;

  base::HistogramTester histogram_tester_;
};

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdCommandLineOverride) {
  // The test is set up to use the command line to simulate the country as being
  // Belgium.
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);

  // When removing the command line flag, the default value is based on the
  // device locale.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // Note that if the format matches (2-character strings), we might get a
  // country ID that is not valid/supported.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "??");
  EXPECT_NE(GetCountryId(), country_codes::kCountryIDUnknown);
  EXPECT_EQ(GetCountryId(), country_codes::CountryCharsToCountryID('?', '?'));
}

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryIdCommandLineOverrideSetsToUnknownOnFormatMismatch) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // When the command line value is invalid, the country code should be unknown.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "USA");
  EXPECT_EQ(GetCountryId(), country_codes::kCountryIDUnknown);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(RegionalCapabilitiesServiceTest, PlayResponseBeforeGetCountryId) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  TestSupportAndroid test_support;
  test_support.ReturnDeviceCountry(
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // We got response from Play API before `GetCountryId` was invoked for the
  // first time this run, so the new value should be used right away.
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);
  // The pref should be updated as well.
  EXPECT_EQ(pref_service().GetInteger(country_codes::kCountryIDAtInstall),
            kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdBeforePlayResponse) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  TestSupportAndroid test_support;
  // We didn't get a response from Play API before `GetCountryId` was invoked,
  // so the last known country from prefs should be used.
  EXPECT_EQ(GetCountryId(), country_codes::GetCurrentCountryID());

  // Simulate a response arriving after the first `GetCountryId` call.
  test_support.ReturnDeviceCountry(
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // The pref should be updated so the new country can be used the next run.
  EXPECT_EQ(pref_service().GetInteger(country_codes::kCountryIDAtInstall),
            kBelgiumCountryId);
  // However, `GetCountryId` result shouldn't change until the next run.
  EXPECT_EQ(GetCountryId(), country_codes::GetCurrentCountryID());
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdPrefAlreadyWritten) {
  // The value set from the pref should be used.
  pref_service().SetInteger(country_codes::kCountryIDAtInstall,
                            kBelgiumCountryId);
  // Don't create `TestSupportAndroid` - since the pref isn't set we should not
  // reach out to Java.
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);
}
#else
// On Android, internal device APIs are used to get the current country.
TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdDefault) {
  // Remove the command line flag set by the test.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // The default value should be based on the device locale.
  EXPECT_EQ(GetCountryId(), country_codes::GetCurrentCountryID());
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdFromPrefs) {
  // Remove the command line flag set by the test.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // The value set from the pref should be used.
  pref_service().SetInteger(country_codes::kCountryIDAtInstall,
                            kBelgiumCountryId);
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdChangesAfterReading) {
  // Remove the command line flag set by the test.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // The value set from the pref should be used.
  pref_service().SetInteger(country_codes::kCountryIDAtInstall,
                            kBelgiumCountryId);
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);

  // Change the value in pref.
  pref_service().SetInteger(country_codes::kCountryIDAtInstall,
                            country_codes::CountryCharsToCountryID('U', 'S'));
  // The value returned by `GetCountryId` shouldn't change.
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST_F(RegionalCapabilitiesServiceTest, ClearPrefForUnknownCountry) {
#if BUILDFLAG(IS_ANDROID)
  TestSupportAndroid test_support;
  test_support.ReturnDeviceCountry(
      country_codes::CountryIDToCountryString(kBelgiumCountryId));
#endif
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kClearPrefForUnknownCountry};
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  InitService(kBelgiumCountryId);
  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  pref_service().SetInteger(country_codes::kCountryIDAtInstall,
                            country_codes::kCountryIDUnknown);
  EXPECT_EQ(GetCountryId(), kBelgiumCountryId);
  histogram_tester().ExpectBucketCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 2, 1);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_LINUX)

}  // namespace regional_capabilities
