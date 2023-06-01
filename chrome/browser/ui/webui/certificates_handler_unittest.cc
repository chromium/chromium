// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificates_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_tags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class CertificateHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    base::AddTagToTestResult("feature_id",
                             "screenplay-7c74e36b-7675-4fa7-91ca-24577bb37203");
    ChromeRenderViewHostTestHarness::SetUp();

    web_ui_.set_web_contents(web_contents());
    cert_handler_.set_web_ui(&web_ui_);
    pref_service_ = profile()->GetTestingPrefService();
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool IsCACertificateManagementAllowedPolicy(CertificateSource source) {
    return cert_handler_.IsCACertificateManagementAllowedPolicy(source);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool CanDeleteCertificate(
      const CertificateManagerModel::CertInfo* cert_info) {
    return cert_handler_.CanDeleteCertificate(cert_info);
  }

  bool CanEditCertificate(const CertificateManagerModel::CertInfo* cert_info) {
    return cert_handler_.CanEditCertificate(cert_info);
  }

 protected:
  content::TestWebUI web_ui_;
  certificate_manager::CertificatesHandler cert_handler_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable, DanglingUntriaged>
      pref_service_ = nullptr;
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(CertificateHandlerTest, IsCACertificateManagementAllowedPolicyTest) {
  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kAll));

    EXPECT_TRUE(
        IsCACertificateManagementAllowedPolicy(CertificateSource::kImported));
    EXPECT_TRUE(
        IsCACertificateManagementAllowedPolicy(CertificateSource::kBuiltIn));
  }

  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kUserOnly));

    EXPECT_TRUE(
        IsCACertificateManagementAllowedPolicy(CertificateSource::kImported));
    EXPECT_FALSE(
        IsCACertificateManagementAllowedPolicy(CertificateSource::kBuiltIn));
  }

  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kNone));

    EXPECT_FALSE(
        IsCACertificateManagementAllowedPolicy(CertificateSource::kImported));
    EXPECT_FALSE(
        IsCACertificateManagementAllowedPolicy(CertificateSource::kBuiltIn));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(CertificateHandlerTest, CanDeleteCertificateCommonTest) {
  CertificateManagerModel::CertInfo default_cert_info(
      {} /* cert */, net::CertType::USER_CERT, {} /* cert_name */,
      false /* can_be_deleted */, false /* untrusted */,
      CertificateManagerModel::CertInfo::Source::kPolicy,
      true /* web_trust_anchor */, false /* hardware_backed */,
      false /* device_wide */);

  {
    auto cert_info =
        CertificateManagerModel::CertInfo::Clone(&default_cert_info);
    cert_info->type_ = net::CertType::USER_CERT;
    cert_info->can_be_deleted_ = false;
    cert_info->source_ = CertificateManagerModel::CertInfo::Source::kExtension;

    // Deletion of |!can_be_deleted_| certificates is not allowed.
    EXPECT_FALSE(CanDeleteCertificate(cert_info.get()));
  }

  {
    auto cert_info =
        CertificateManagerModel::CertInfo::Clone(&default_cert_info);
    cert_info->type_ = net::CertType::USER_CERT;
    cert_info->can_be_deleted_ = true;
    cert_info->source_ = CertificateManagerModel::CertInfo::Source::kPolicy;

    // Deletion of policy certificates is not allowed.
    EXPECT_FALSE(CanDeleteCertificate(cert_info.get()));
  }
}

TEST_F(CertificateHandlerTest, CanDeleteUserCertificateTest) {
  CertificateManagerModel::CertInfo cert_info(
      {} /* cert */, net::CertType::USER_CERT, {} /* cert_name */,
      true /* can_be_deleted */, false /* untrusted */,
      CertificateManagerModel::CertInfo::Source::kExtension,
      true /* web_trust_anchor */, false /* hardware_backed */,
      false /* device_wide */);
  {
    cert_info.device_wide_ = false;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));
  }

#if BUILDFLAG(IS_CHROMEOS)
  {
    pref_service_->SetInteger(
        prefs::kClientCertificateManagementAllowed,
        static_cast<int>(ClientCertificateManagementPermission::kAll));

    cert_info.device_wide_ = false;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kClientCertificateManagementAllowed,
        static_cast<int>(ClientCertificateManagementPermission::kUserOnly));

    cert_info.device_wide_ = false;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kClientCertificateManagementAllowed,
        static_cast<int>(ClientCertificateManagementPermission::kNone));

    cert_info.device_wide_ = false;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(CertificateHandlerTest, CanDeleteCACertificateTest) {
  CertificateManagerModel::CertInfo cert_info(
      {} /* cert */, net::CertType::CA_CERT, {} /* cert_name */,
      true /* can_be_deleted */, false /* untrusted */,
      CertificateManagerModel::CertInfo::Source::kExtension,
      true /* web_trust_anchor */, false /* hardware_backed */,
      false /* device_wide */);
  {
    cert_info.can_be_deleted_ = false;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));
  }

#if BUILDFLAG(IS_CHROMEOS)
  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kAll));

    cert_info.can_be_deleted_ = false;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kUserOnly));

    cert_info.can_be_deleted_ = false;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_TRUE(CanDeleteCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kNone));

    cert_info.can_be_deleted_ = false;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));
    cert_info.can_be_deleted_ = true;
    EXPECT_FALSE(CanDeleteCertificate(&cert_info));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(CertificateHandlerTest, CanEditCertificateCommonTest) {
  CertificateManagerModel::CertInfo cert_info(
      {} /* cert */, net::CertType::USER_CERT, {} /* cert_name */,
      true /* can_be_deleted */, false /* untrusted */,
      CertificateManagerModel::CertInfo::Source::kExtension,
      true /* web_trust_anchor */, false /* hardware_backed */,
      false /* device_wide */);

  cert_info.source_ = CertificateManagerModel::CertInfo::Source::kExtension;
  cert_info.type_ = net::CertType::USER_CERT;
  EXPECT_FALSE(CanEditCertificate(&cert_info));

  cert_info.source_ = CertificateManagerModel::CertInfo::Source::kExtension;
  cert_info.type_ = net::CertType::SERVER_CERT;
  EXPECT_FALSE(CanEditCertificate(&cert_info));

  cert_info.source_ = CertificateManagerModel::CertInfo::Source::kExtension;
  cert_info.type_ = net::CertType::OTHER_CERT;
  EXPECT_FALSE(CanEditCertificate(&cert_info));

  cert_info.source_ = CertificateManagerModel::CertInfo::Source::kPolicy;
  cert_info.type_ = net::CertType::CA_CERT;
  EXPECT_FALSE(CanEditCertificate(&cert_info));
}

// Edit of user certificates is not allowed in any case.
TEST_F(CertificateHandlerTest, CanEditUserCertificateTest) {
  CertificateManagerModel::CertInfo cert_info(
      {} /* cert */, net::CertType::USER_CERT, {} /* cert_name */,
      true /* can_be_deleted */, false /* untrusted */,
      CertificateManagerModel::CertInfo::Source::kExtension,
      true /* web_trust_anchor */, false /* hardware_backed */,
      false /* device_wide */);
  {
    cert_info.device_wide_ = false;
    EXPECT_FALSE(CanEditCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }

#if BUILDFLAG(IS_CHROMEOS)
  {
    pref_service_->SetInteger(
        prefs::kClientCertificateManagementAllowed,
        static_cast<int>(ClientCertificateManagementPermission::kAll));

    cert_info.device_wide_ = false;
    EXPECT_FALSE(CanEditCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kClientCertificateManagementAllowed,
        static_cast<int>(ClientCertificateManagementPermission::kUserOnly));

    cert_info.device_wide_ = false;
    EXPECT_FALSE(CanEditCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kClientCertificateManagementAllowed,
        static_cast<int>(ClientCertificateManagementPermission::kNone));

    cert_info.device_wide_ = false;
    EXPECT_FALSE(CanEditCertificate(&cert_info));

    cert_info.device_wide_ = true;
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(CertificateHandlerTest, CanEditCACertificateTest) {
  CertificateManagerModel::CertInfo cert_info(
      {} /* cert */, net::CertType::CA_CERT, {} /* cert_name */,
      false /* can_be_deleted */, false /* untrusted */,
      CertificateManagerModel::CertInfo::Source::kExtension,
      true /* web_trust_anchor */, false /* hardware_backed */,
      false /* device_wide */);
  {
    cert_info.can_be_deleted_ = false;
    EXPECT_TRUE(CanEditCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_TRUE(CanEditCertificate(&cert_info));
  }

#if BUILDFLAG(IS_CHROMEOS)
  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kAll));

    cert_info.can_be_deleted_ = false;
    EXPECT_TRUE(CanEditCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_TRUE(CanEditCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kUserOnly));

    cert_info.can_be_deleted_ = false;
    EXPECT_FALSE(CanEditCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_TRUE(CanEditCertificate(&cert_info));
  }

  {
    pref_service_->SetInteger(
        prefs::kCACertificateManagementAllowed,
        static_cast<int>(CACertificateManagementPermission::kNone));

    cert_info.can_be_deleted_ = false;
    EXPECT_FALSE(CanEditCertificate(&cert_info));

    cert_info.can_be_deleted_ = true;
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}


#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(CertificateHandlerTest, CannotManageFromSecondaryLacrosProfileTest) {
  profile()->SetIsMainProfile(false);

  pref_service_->SetInteger(
      prefs::kCACertificateManagementAllowed,
      static_cast<int>(CACertificateManagementPermission::kAll));
  pref_service_->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kAll));

  EXPECT_FALSE(
      IsCACertificateManagementAllowedPolicy(CertificateSource::kImported));
  EXPECT_FALSE(
      IsCACertificateManagementAllowedPolicy(CertificateSource::kBuiltIn));

  {
    CertificateManagerModel::CertInfo cert_info(
        {} /* cert */, net::CertType::USER_CERT, {} /* cert_name */,
        true /* can_be_deleted */, false /* untrusted */,
        CertificateManagerModel::CertInfo::Source::kPolicy,
        true /* web_trust_anchor */, false /* hardware_backed */,
        false /* device_wide */);

    EXPECT_FALSE(CanDeleteCertificate(&cert_info));
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }

  {
    CertificateManagerModel::CertInfo cert_info(
        {} /* cert */, net::CertType::CA_CERT, {} /* cert_name */,
        true /* can_be_deleted */, false /* untrusted */,
        CertificateManagerModel::CertInfo::Source::kPolicy,
        true /* web_trust_anchor */, false /* hardware_backed */,
        false /* device_wide */);

    EXPECT_FALSE(CanDeleteCertificate(&cert_info));
    EXPECT_FALSE(CanEditCertificate(&cert_info));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
