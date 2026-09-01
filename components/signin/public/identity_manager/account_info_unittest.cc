// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_info.h"

#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/signin/public/android/jni_headers/AccountInfo_jni.h"
#include "components/signin/public/android/jni_headers/CoreAccountInfo_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#endif

using signin::constants::kNoHostedDomainFound;

class AccountInfoTest : public testing::Test {};

TEST_F(AccountInfoTest, IsEmpty) {
  {
    AccountInfo info_empty;
    EXPECT_TRUE(info_empty.IsEmpty());
  }
  {
    CoreAccountInfo core_info;
    core_info.account_id = CoreAccountId::FromGaiaId(GaiaId("test_id"));
    AccountInfo info_with_account_id = AccountInfo::Builder(core_info).Build();
    EXPECT_FALSE(info_with_account_id.IsEmpty());
  }
  {
    CoreAccountInfo core_info;
    core_info.email = "test_email@email.com";
    AccountInfo info_with_email = AccountInfo::Builder(core_info).Build();
    EXPECT_FALSE(info_with_email.IsEmpty());
  }
  {
    CoreAccountInfo core_info;
    core_info.gaia = GaiaId("test_gaia");
    AccountInfo info_with_gaia = AccountInfo::Builder(core_info).Build();
    EXPECT_FALSE(info_with_gaia.IsEmpty());
  }
}

TEST_F(AccountInfoTest, GetCoreAccountInfo) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsUnderAdvancedProtection(true)
          .Build();

  const CoreAccountInfo& core_info = info;
  EXPECT_EQ(core_info.account_id, CoreAccountId::FromGaiaId(GaiaId("test_id")));
  EXPECT_EQ(core_info.gaia, GaiaId("test_id"));
  EXPECT_EQ(core_info.email, "test@example.com");
  EXPECT_TRUE(core_info.is_under_advanced_protection);
}

TEST_F(AccountInfoTest, DefaultIsInvalid) {
  AccountInfo empty_info;
  EXPECT_EQ(empty_info.IsChildAccount(), signin::Tribool::kUnknown);
  EXPECT_FALSE(empty_info.IsValid());
}

// Tests that IsValid() returns true when all mandatory fields are non-empty.
TEST_F(AccountInfoTest, IsValid) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test_id")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetFullName("test_name")
          .SetGivenName("test_name")
          .SetHostedDomain("test_domain")
          .SetAvatarUrl("test_picture_url")
          .Build();
  EXPECT_TRUE(info.IsValid());
}

// Tests that UpdateWith() correctly ignores parameters with a different
// account ID.
TEST_F(AccountInfoTest, UpdateWithDifferentAccountId) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .Build();

  const GaiaId other_gaia_id = GaiaId("test_other_id");
  AccountInfo other =
      AccountInfo::Builder(other_gaia_id, "test_other@email.org")
          .SetAccountId(CoreAccountId::FromGaiaId(other_gaia_id))
          .Build();

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ(info.GetGaiaId(), GaiaId("test_id"));
  EXPECT_EQ(info.GetEmail(), "test@example.com");
}

// Tests that UpdateWith() doesn't update the fields that were already set
// to the correct value.
TEST_F(AccountInfoTest, UpdateWithNoModification) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsChildAccount(signin::Tribool::kTrue)
          .SetIsUnderAdvancedProtection(true)
          .SetLocale("en")
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSettings)
#endif
          .Build();

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsUnderAdvancedProtection(false)
          .SetLocale("en")
          .Build();
  EXPECT_EQ(other.IsChildAccount(), signin::Tribool::kUnknown);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_FALSE(other.GetLastAuthenticationAccessPoint().has_value());
#endif

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ(info.GetGaiaId(), GaiaId("test_id"));
  EXPECT_EQ(info.GetEmail(), "test@example.com");
  EXPECT_EQ(info.GetLocale(), "en");
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(info.GetLastAuthenticationAccessPoint().has_value());
  EXPECT_EQ(info.GetLastAuthenticationAccessPoint().value(),
            signin_metrics::AccessPoint::kSettings);
#endif
  EXPECT_EQ(info.IsChildAccount(), signin::Tribool::kTrue);
  EXPECT_TRUE(info.IsUnderAdvancedProtection());
}

// Tests that UpdateWith() correctly updates its fields that were not set.
TEST_F(AccountInfoTest, UpdateWithSuccessfulUpdate) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .Build();

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetFullName("test_name")
          .SetGivenName("test_name")
          .SetLocale("fr")
          .SetIsChildAccount(signin::Tribool::kTrue)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSettings)
#endif
          .Build();
  AccountCapabilitiesTestMutator mutator(&other);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  EXPECT_TRUE(info.UpdateWith(other));
  EXPECT_EQ(info.GetGaiaId(), GaiaId("test_id"));
  EXPECT_EQ(info.GetEmail(), "test@example.com");
  EXPECT_EQ(info.GetFullName(), "test_name");
  EXPECT_EQ(info.GetGivenName(), "test_name");
  EXPECT_EQ(info.GetLocale(), "fr");
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(info.GetLastAuthenticationAccessPoint().has_value());
  EXPECT_EQ(info.GetLastAuthenticationAccessPoint().value(),
            signin_metrics::AccessPoint::kSettings);
#endif
  EXPECT_EQ(info.IsChildAccount(), signin::Tribool::kTrue);
  EXPECT_EQ(
      info.GetAccountCapabilities()
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kTrue);
}

// Tests that UpdateWith() sets default values for hosted_domain and
// picture_url if the properties are unset.
TEST_F(AccountInfoTest, UpdateWithDefaultValues) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .Build();

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetHostedDomain(std::string())
          .SetAvatarUrl(kNoPictureURLFound)
          .Build();

  EXPECT_TRUE(info.UpdateWith(other));
  EXPECT_EQ(info.GetHostedDomain(), "");
  EXPECT_EQ(info.GetAvatarUrl(), "");
}

// Tests that UpdateWith() ignores default values for hosted_domain and
// picture_url if they are already set.
TEST_F(AccountInfoTest, UpdateWithDefaultValuesNoOverride) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetHostedDomain("test_domain")
          .SetAvatarUrl("test_url")
          .Build();
  AccountCapabilitiesTestMutator(&info).set_is_subject_to_enterprise_features(
      true);

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetHostedDomain(std::string())
          .SetAvatarUrl(kNoPictureURLFound)
          .Build();

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ(info.GetHostedDomain(), "test_domain");
  EXPECT_EQ(info.GetAvatarUrl(), "test_url");
}

TEST_F(AccountInfoTest, BuilderPopulatesCoreAccountInfoFields) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsUnderAdvancedProtection(true)
          .Build();

  EXPECT_EQ(info.GetGaiaId(), GaiaId("test_id"));
  EXPECT_EQ(info.GetEmail(), "test@example.com");
  EXPECT_EQ(info.GetAccountId(), CoreAccountId::FromGaiaId(GaiaId("test_id")));
  EXPECT_TRUE(info.IsUnderAdvancedProtection());
}

TEST_F(AccountInfoTest, BuilderSetsAccountIdFromGaiaIdWithEnforcement) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kGaiaAccountIdEnforcement);

  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com").Build();

  EXPECT_EQ(info.GetGaiaId(), GaiaId("test_id"));
  EXPECT_EQ(info.GetEmail(), "test@example.com");
  EXPECT_EQ(info.GetAccountId(), CoreAccountId::FromGaiaId(GaiaId("test_id")));
}

TEST_F(AccountInfoTest, GettersEmptyAccountInfo) {
  AccountInfo info;
  EXPECT_EQ(info.GetFullName(), std::nullopt);
  EXPECT_EQ(info.GetGivenName(), std::nullopt);
  EXPECT_EQ(info.GetHostedDomain(), std::nullopt);
  EXPECT_EQ(info.GetAvatarUrl(), std::nullopt);
  EXPECT_EQ(info.GetLastDownloadedAvatarUrlWithSize(), std::nullopt);
  EXPECT_EQ(info.GetAvatarImage(), std::nullopt);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_FALSE(info.GetLastAuthenticationAccessPoint().has_value());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_FALSE(info.GetAccountCapabilities().AreAnyCapabilitiesKnown());
  EXPECT_EQ(info.IsChildAccount(), signin::Tribool::kUnknown);
  EXPECT_EQ(info.GetLocale(), std::nullopt);
}

TEST_F(AccountInfoTest, GettersPopulatedAccountInfo) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetFullName("full_name")
          .SetGivenName("given_name")
          .SetHostedDomain("hosted_domain")
          .SetAvatarUrl("picture_url")
          .SetLastDownloadedAvatarUrlWithSize("picture_url_with_size")
          .SetAvatarImage(gfx::test::CreateImage(/*size*/ 24))
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSettings)
#endif
          .UpdateAccountCapabilitiesWith(capabilities)
          .SetIsChildAccount(signin::Tribool::kFalse)
          .SetLocale("fr")
          .Build();

  EXPECT_EQ(info.GetFullName(), "full_name");
  EXPECT_EQ(info.GetGivenName(), "given_name");
  EXPECT_EQ(info.GetHostedDomain(), "hosted_domain");
  EXPECT_EQ(info.GetAvatarUrl(), "picture_url");
  EXPECT_EQ(info.GetLastDownloadedAvatarUrlWithSize(), "picture_url_with_size");
  EXPECT_NE(info.GetAvatarImage(), std::nullopt);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(info.GetLastAuthenticationAccessPoint().has_value());
  EXPECT_EQ(info.GetLastAuthenticationAccessPoint().value(),
            signin_metrics::AccessPoint::kSettings);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(info.GetAccountCapabilities().AreAnyCapabilitiesKnown());
  EXPECT_EQ(info.IsChildAccount(), signin::Tribool::kFalse);
  EXPECT_EQ(info.GetLocale(), "fr");
}

TEST_F(AccountInfoTest, SetAvatarImage) {
  AccountInfo info = AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
                         .SetAvatarImage(gfx::test::CreateImage(/*size=*/24))
                         .Build();
  EXPECT_NE(info.GetAvatarImage(), std::nullopt);
  EXPECT_EQ(info.GetAvatarImage()->Width(), 24);
  EXPECT_EQ(info.GetAvatarImage()->Height(), 24);
}

TEST_F(AccountInfoTest, DeprecatedSentinelValues) {
  AccountInfo info = AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
                         .SetHostedDomain(kNoHostedDomainFound)
                         .SetAvatarUrl(kNoPictureURLFound)
                         .Build();

  EXPECT_EQ(info.GetHostedDomain(), std::string());
  EXPECT_EQ(info.GetAvatarUrl(), std::string());
}

TEST_F(AccountInfoTest, EmptyTheSameAsDeprecatedSentinelValues) {
  AccountInfo info = AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
                         .SetHostedDomain(std::string())
                         .SetAvatarUrl(std::string())
                         .Build();

  EXPECT_EQ(info.GetHostedDomain(), std::string());
  EXPECT_EQ(info.GetAvatarUrl(), std::string());
}

TEST_F(AccountInfoTest, CreateWithPossiblyEmptyGaiaId) {
  // TODO(crbug.com/502237328): Remove this test after launching
  // kGaiaAccountIdEnforcement.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kGaiaAccountIdEnforcement);

  AccountInfo info = AccountInfo::Builder::CreateWithPossiblyEmptyGaiaId(
                         GaiaId(), "test@example.org")
                         .Build();

  EXPECT_TRUE(info.GetGaiaId().empty());
  EXPECT_EQ(info.GetEmail(), "test@example.org");
}

TEST_F(AccountInfoTest, CreateWithPossiblyEmptyGaiaIdAndEmail) {
  // TODO(crbug.com/40283608): Remove this test after seeding incomplete
  // accounts is stopped.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kGaiaAccountIdEnforcement);

  AccountInfo info_empty_gaia =
      AccountInfo::Builder::CreateWithPossiblyEmptyGaiaIdAndEmail(
          GaiaId(), "test@example.org")
          .Build();
  EXPECT_TRUE(info_empty_gaia.GetGaiaId().empty());
  EXPECT_EQ(info_empty_gaia.GetEmail(), "test@example.org");

  AccountInfo info_empty_email =
      AccountInfo::Builder::CreateWithPossiblyEmptyGaiaIdAndEmail(
          GaiaId("test_gaia_id"), "")
          .Build();
  EXPECT_EQ(info_empty_email.GetGaiaId(), GaiaId("test_gaia_id"));
  EXPECT_TRUE(info_empty_email.GetEmail().empty());

  AccountInfo info_empty_both =
      AccountInfo::Builder::CreateWithPossiblyEmptyGaiaIdAndEmail(GaiaId(), "")
          .Build();
  EXPECT_TRUE(info_empty_both.GetGaiaId().empty());
  EXPECT_TRUE(info_empty_both.GetEmail().empty());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AccountInfoTest, ConvertCoreAccountInfoToJavaCoreAccountInfo) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CoreAccountInfo core_account_info;
  core_account_info.account_id =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  core_account_info.gaia = GaiaId("test_gaia_id");
  core_account_info.email = "test_email@example.com";

  base::android::ScopedJavaLocalRef<jobject> j_core_account_info =
      ConvertToJavaCoreAccountInfo(env, core_account_info);

  EXPECT_EQ(signin::Java_CoreAccountInfo_getGaiaId(env, j_core_account_info),
            GaiaId("test_gaia_id"));
  EXPECT_EQ(signin::Java_CoreAccountInfo_getEmail(env, j_core_account_info),
            "test_email@example.com");
  EXPECT_EQ(signin::Java_CoreAccountInfo_getId(env, j_core_account_info),
            CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")));
}

TEST_F(AccountInfoTest, ConvertAccountInfoToJavaAccountInfo) {
  JNIEnv* env = base::android::AttachCurrentThread();
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.SetAllSupportedCapabilities(true);
  AccountInfo account_info =
      AccountInfo::Builder(GaiaId("test_gaia_id"), "test_email@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")))
          .SetFullName("test_full_name")
          .SetGivenName("test_given_name")
          .SetHostedDomain("test_hosted_domain")
          .SetAvatarImage(gfx::test::CreateImage(250, 150))
          .UpdateAccountCapabilitiesWith(capabilities)
          .Build();

  base::android::ScopedJavaLocalRef<jobject> j_account_info =
      ConvertToJavaAccountInfo(env, account_info);

  EXPECT_EQ(signin::Java_CoreAccountInfo_getGaiaId(env, j_account_info),
            GaiaId("test_gaia_id"));
  EXPECT_EQ(signin::Java_CoreAccountInfo_getEmail(env, j_account_info),
            "test_email@example.com");
  EXPECT_EQ(signin::Java_CoreAccountInfo_getId(env, j_account_info),
            CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")));
  EXPECT_EQ(signin::Java_AccountInfo_getFullName(env, j_account_info),
            "test_full_name");
  EXPECT_EQ(signin::Java_AccountInfo_getGivenName(env, j_account_info),
            "test_given_name");
  EXPECT_EQ(
      base::android::ConvertJavaStringToUTF8(
          signin::Java_AccountInfo_getRawHostedDomain(env, j_account_info)),
      "test_hosted_domain");
  EXPECT_EQ(AccountCapabilities::ConvertFromJavaAccountCapabilities(
                env, signin::Java_AccountInfo_getAccountCapabilities(
                         env, j_account_info)),
            capabilities);
  EXPECT_FALSE(
      signin::Java_AccountInfo_getAccountImage(env, j_account_info).is_null());
}

TEST_F(AccountInfoTest, ConvertFromJavaCoreAccountInfo) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> coreAccountInfo =
      signin::Java_CoreAccountInfo_Constructor(
          env, CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
          "test_email@example.com", GaiaId("test_gaia_id"));

  CoreAccountInfo result = ConvertFromJavaCoreAccountInfo(env, coreAccountInfo);

  EXPECT_EQ(result.account_id,
            CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")));
  EXPECT_EQ(result.gaia, GaiaId("test_gaia_id"));
  EXPECT_EQ(result.email, "test_email@example.com");
}

TEST_F(AccountInfoTest, ConvertFromJavaAccountInfo) {
  JNIEnv* env = base::android::AttachCurrentThread();
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.SetAllSupportedCapabilities(true);

  base::android::ScopedJavaLocalRef<jobject> accountInfo =
      signin::Java_AccountInfo_Constructor(
          env, CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
          "test_email@example.com", GaiaId("test_gaia_id"), "test_full_name",
          "test_given_name",
          base::android::ConvertUTF8ToJavaString(env, "test_hosted_domain"),
          gfx::ConvertToJavaBitmap(
              *gfx::test::CreateImage(250, 150).AsImageSkia().bitmap()),
          capabilities.ConvertToJavaAccountCapabilities(env));

  AccountInfo result = ConvertFromJavaAccountInfo(env, accountInfo);

  EXPECT_EQ(result.GetGaiaId(), GaiaId("test_gaia_id"));
  EXPECT_EQ(result.GetEmail(), "test_email@example.com");
  EXPECT_EQ(result.GetAccountId(),
            CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")));
  EXPECT_EQ(result.GetFullName(), "test_full_name");
  EXPECT_EQ(result.GetGivenName(), "test_given_name");
  EXPECT_EQ(result.GetHostedDomain(), "test_hosted_domain");
  EXPECT_FALSE(result.GetAvatarImage()->IsEmpty());
  EXPECT_EQ(result.GetAvatarImage()->Width(), 250);
  EXPECT_EQ(result.GetAvatarImage()->Height(), 150);
  EXPECT_EQ(result.GetAccountCapabilities(), capabilities);
}
#endif
