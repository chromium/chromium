// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pk11pub.h>
#include <secmodt.h>

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "crypto/nss_util_internal.h"
#include "crypto/sha2.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/net/fake_nss_service.h"
#endif

using testing::ElementsAre;

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kUsername[] = "test@example.com";
#endif

bool SlotContainsCertWithHash(PK11SlotInfo* slot, std::string_view hash_hex) {
  if (!slot) {
    return false;
  }
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
  if (!cert_list) {
    return false;
  }
  net::SHA256HashValue hash;
  if (!base::HexStringToSpan(hash_hex, hash)) {
    return false;
  }
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    if (net::x509_util::CalculateFingerprint256(node->cert) == hash) {
      return true;
    }
  }
  return false;
}

std::string HexHash(base::span<const uint8_t> data) {
  return base::ToLowerASCII(base::HexEncode(crypto::SHA256Hash(data)));
}

class FakeCertificateManagerPage
    : public certificate_manager_v2::mojom::CertificateManagerPage {
 public:
  explicit FakeCertificateManagerPage(
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPage>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  void AskForImportPassword(AskForImportPasswordCallback callback) override {
    std::move(callback).Run(password_);
  }

  void AskForConfirmation(const std::string& title,
                          const std::string& message,
                          AskForConfirmationCallback callback) override {
    std::move(callback).Run(ask_for_confirmation_result_);
  }

  void set_trigger_reload_callback(
      base::OnceCallback<
          void(std::vector<certificate_manager_v2::mojom::CertificateSource>)>
          callback) {
    reload_callback_ = std::move(callback);
  }

  void TriggerReload(
      const std::vector<certificate_manager_v2::mojom::CertificateSource>&
          sources) override {
    if (reload_callback_) {
      std::move(reload_callback_).Run(std::move(sources));
    }
  }

  void TriggerMetadataUpdate() override {}

  void set_mocked_import_password(std::optional<std::string> password) {
    password_ = std::move(password);
  }

  void set_mocked_confirmation_result(bool result) {
    ask_for_confirmation_result_ = result;
  }

 private:
  std::optional<std::string> password_;
  bool ask_for_confirmation_result_ = false;
  mojo::Receiver<certificate_manager_v2::mojom::CertificateManagerPage>
      receiver_;
  base::OnceCallback<void(
      std::vector<certificate_manager_v2::mojom::CertificateSource>)>
      reload_callback_;
};

}  // namespace

class ClientCertSourceWritableUnitTest
    : public ChromeRenderViewHostTestHarness,
#if BUILDFLAG(IS_CHROMEOS)
      public testing::WithParamInterface<std::tuple<bool, bool, bool>>
#else
      // In the non-ChromeOS case, the test does not actually need any
      // parameters, but to allow more commonality between the platforms keep
      // it as a parameterized test with a single param that is ignored.
      public testing::WithParamInterface<bool>
#endif
{
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

#if BUILDFLAG(IS_CHROMEOS)
    ASSERT_TRUE(test_nss_user_.constructed_successfully());
    test_nss_user_.FinishInit();

    feature_list_.InitWithFeatureStates(
        {{chromeos::features::kEnablePkcs12ToChapsDualWrite,
          dual_write_enabled()},
         { ash::features::kUseKcerClientCertStore,
           kcer_enabled() }});

    ash::LoginState::Initialize();
    crosapi_manager_ = std::make_unique<crosapi::CrosapiManager>();
#endif

    ChromeRenderViewHostTestHarness::SetUp();

#if BUILDFLAG(IS_CHROMEOS)
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    // is_affiliated=true is required for nss_service_chromeos to configure the
    // system slot.
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_, /*is_affiliated=*/true, user_manager::UserType::kRegular,
        profile());
    fake_user_manager_->OnUserProfileCreated(account_, profile()->GetPrefs());
    fake_user_manager_->LoginUser(account_);
#else
    nss_service_ = FakeNssService::InitializeForBrowserContext(profile());
#endif

    fake_page_ = std::make_unique<FakeCertificateManagerPage>(
        fake_page_remote_.BindNewPipeAndPassReceiver());

    cert_source_ =
        CreatePlatformClientCertSource(&fake_page_remote_, profile());
  }

  void TearDown() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    cert_source_.reset();
#if BUILDFLAG(IS_CHROMEOS)
    fake_user_manager_.Reset();
    crosapi_manager_.reset();
    ash::LoginState::Shutdown();
    kcer::KcerFactoryAsh::ClearNssTokenMapForTesting();
#else
    nss_service_ = nullptr;
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool dual_write_enabled() const { return std::get<0>(GetParam()); }
  bool kcer_enabled() const { return std::get<1>(GetParam()); }
  bool use_hardware_backed() const { return std::get<2>(GetParam()); }

  std::string username_hash() const {
    return user_manager::FakeUserManager::GetFakeUsernameHash(account_);
  }
#endif

  void DoImport(
      CertificateManagerPageHandler::ImportCertificateCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
    if (use_hardware_backed()) {
      cert_source_->ImportAndBindCertificate(web_contents()->GetWeakPtr(),
                                             std::move(callback));
    } else {
      cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                      std::move(callback));
    }
#else
    cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                    std::move(callback));
#endif
  }

  std::optional<certificate_manager_v2::mojom::SummaryCertInfoPtr>
  GetCertificateInfosForCertHash(std::string_view hash_hex) const {
    base::test::TestFuture<
        std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr>>
        get_certs_waiter;
    cert_source_->GetCertificateInfos(get_certs_waiter.GetCallback());
    const auto& certs = get_certs_waiter.Get();
    for (const auto& cert : certs) {
      if (cert->sha256hash_hex == hash_hex) {
        return cert.Clone();
      }
    }
    return std::nullopt;
  }

  bool GetCertificateInfosIsCertDeletable(std::string_view hash_hex) const {
    auto cert = GetCertificateInfosForCertHash(hash_hex);
    if (!cert.has_value()) {
      ADD_FAILURE();
      return false;
    }
    return cert.value()->is_deletable;
  }

  bool GetCertificateInfosContainsCertWithHash(
      std::string_view hash_hex) const {
    return GetCertificateInfosForCertHash(hash_hex).has_value();
  }

  // Returns the hex-encoded SHA-256 hash of the client cert that was imported
  // or empty string on error.
  std::string GetTestCertHash(base::FilePath file_path) {
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(file_path);
    if (!cert) {
      ADD_FAILURE() << "error reading " << file_path.AsUTF8Unsafe();
      return {};
    }

    return HexHash(cert->cert_span());
  }

  // Imports a client certificate directly, without going through the UI or
  // checking the management allowed policy. Returns the hex-encoded SHA-256
  // hash of the client cert that was imported or empty string on error.
  std::string ImportForTesting(base::FilePath p12_file_path,
                               std::string_view password,
                               bool import_to_system_slot) {
    base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
    NssServiceFactory::GetForContext(profile())
        ->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
    net::NSSCertDatabase* nss_db = nss_waiter.Get();

    crypto::ScopedPK11Slot slot;
#if BUILDFLAG(IS_CHROMEOS)
    if (import_to_system_slot) {
      slot = nss_db->GetSystemSlot();
    } else if (use_hardware_backed()) {
      slot = nss_db->GetPrivateSlot();
    } else {
      slot = nss_db->GetPublicSlot();
    }
#else
    slot = crypto::ScopedPK11Slot(
        PK11_ReferenceSlot(nss_service_->GetPublicSlot()));
#endif

    std::string p12_file_data;
    if (!base::ReadFileToString(p12_file_path, &p12_file_data)) {
      ADD_FAILURE() << "error reading " << p12_file_path.AsUTF8Unsafe();
      return {};
    }

    net::ScopedCERTCertificateList imported_certs;
    int r = nss_db->ImportFromPKCS12(slot.get(), std::move(p12_file_data),
                                     base::UTF8ToUTF16(password),
                                     /*is_extractable=*/true, &imported_certs);
    if (r != net::OK) {
      ADD_FAILURE() << "NSS import result " << r;
      return {};
    }
    if (imported_certs.size() != 1) {
      ADD_FAILURE() << "NSS imported " << imported_certs.size()
                    << " certs, expected 1";
      return {};
    }

    return HexHash(
        net::x509_util::CERTCertificateAsSpan(imported_certs[0].get()));
  }

  std::string ImportToUserSlotForTesting(base::FilePath file_path,
                                         std::string_view password) {
    return ImportForTesting(file_path, password,
                            /*import_to_system_slot=*/false);
  }

  std::string ImportToSystemSlotForTesting(base::FilePath file_path,
                                           std::string_view password) {
    return ImportForTesting(file_path, password,
                            /*import_to_system_slot=*/true);
  }

  bool NSSContainsCertWithHash(std::string_view hash_hex) {
#if BUILDFLAG(IS_CHROMEOS)
    return SlotContainsCertWithHash(
        crypto::GetPublicSlotForChromeOSUser(username_hash()).get(), hash_hex);
#else
    return SlotContainsCertWithHash(nss_service_->GetPublicSlot(), hash_hex);
#endif
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList feature_list_;
  AccountId account_{AccountId::FromUserEmail(kUsername)};
  crypto::ScopedTestNSSChromeOSUser test_nss_user_{username_hash()};
  crypto::ScopedTestSystemNSSKeySlot test_nss_system_slot_{
      /*simulate_token_loader=*/true};

  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
#else
  raw_ptr<FakeNssService> nss_service_;
#endif

  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote_;
  std::unique_ptr<FakeCertificateManagerPage> fake_page_;
  std::unique_ptr<CertificateManagerPageHandler::CertSource> cert_source_;
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(ClientCertSourceWritableUnitTest, TriggerReloadOnKcerDbChange) {
  if (!kcer_enabled()) {
    return;
  }
  base::test::TestFuture<
      std::vector<certificate_manager_v2::mojom::CertificateSource>>
      reload_future;

  fake_page_->set_trigger_reload_callback(reload_future.GetCallback());
  std::string client_1_hash_hex = ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "chrome");
  ASSERT_FALSE(client_1_hash_hex.empty());

  EXPECT_THAT(reload_future.Get(),
              ElementsAre(certificate_manager_v2::mojom::CertificateSource::
                              kPlatformClientCert));
}
#endif

// Test importing from a PKCS #12 file and then deleting the imported cert,
// with no policy set.
TEST_P(ClientCertSourceWritableUnitTest,
       ImportPkcs12AndGetCertificateInfosAndDelete) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kNssChapsDualWrittenCertsExist));
#endif

  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  std::string client_1_hash_hex =
      GetTestCertHash(net::GetTestCertsDirectory().AppendASCII("client_1.pem"));
  ASSERT_FALSE(client_1_hash_hex.empty());
  EXPECT_FALSE(NSSContainsCertWithHash(client_1_hash_hex));
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));

  {
    // The correct password for the client_1.p12 file.
    fake_page_->set_mocked_import_password("chrome");

    base::test::TestFuture<void> select_file_dialog_opened_waiter;
    factory->SetOpenCallback(
        select_file_dialog_opened_waiter.GetRepeatingCallback());

    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        import_waiter;
    DoImport(import_waiter.GetCallback());
    EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
    ui::FakeSelectFileDialog* fake_file_select_dialog =
        factory->GetLastDialog();
    ASSERT_TRUE(fake_file_select_dialog);
    ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
        net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "p12"));

    certificate_manager_v2::mojom::ActionResultPtr import_result =
        import_waiter.Take();
    ASSERT_TRUE(import_result);
    EXPECT_TRUE(import_result->is_success());
  }

#if BUILDFLAG(IS_CHROMEOS)
  // The cert should be dual written only if dual-write feature is enabled
  // and the import was not hardware backed (if it's hardware backed it
  // already gets imported to Chaps so the dual write isn't needed.)
  EXPECT_EQ(
      profile()->GetPrefs()->GetBoolean(prefs::kNssChapsDualWrittenCertsExist),
      dual_write_enabled() && !use_hardware_backed());
#endif

  EXPECT_TRUE(NSSContainsCertWithHash(client_1_hash_hex));
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(client_1_hash_hex));

  // Now delete the imported certificate, and verify that it is no longer
  // present.
  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_1_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }

  EXPECT_FALSE(NSSContainsCertWithHash(client_1_hash_hex));
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));
}

#if BUILDFLAG(IS_CHROMEOS)

TEST_P(ClientCertSourceWritableUnitTest, PolicyAllAllowsDeletion) {
  std::string client_1_hash_hex = ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "chrome");
  ASSERT_FALSE(client_1_hash_hex.empty());
  std::string client_4_hash_hex = ImportToSystemSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_4.p12"), "chrome");
  ASSERT_FALSE(client_4_hash_hex.empty());

  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kAll));

  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(client_1_hash_hex));

  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_4_hash_hex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(client_4_hash_hex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_1_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_4_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(client_4_hash_hex));
}

TEST_P(ClientCertSourceWritableUnitTest,
       PolicyUserOnlyAllowsDeletionOfUserCertsOnly) {
  std::string client_1_hash_hex = ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "chrome");
  ASSERT_FALSE(client_1_hash_hex.empty());
  std::string client_4_hash_hex = ImportToSystemSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_4.p12"), "chrome");
  ASSERT_FALSE(client_4_hash_hex.empty());

  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kUserOnly));

  // A client certificate in the user slot should be deletable.
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(client_1_hash_hex));

  // A client certificate in the system slot should not be deletable.
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_4_hash_hex));
  if (kcer_enabled()) {
    EXPECT_FALSE(GetCertificateInfosIsCertDeletable(client_4_hash_hex));
  } else {
    // TODO(crbug.com/40928765): the delete button visibility is not set
    // properly when kcer is disabled, for system certs with
    // ClientCertificateManagementAllowed policy set to UserOnly. The policy
    // should still be enforced correctly when actually attempting to delete
    // the certificate below.
    EXPECT_TRUE(GetCertificateInfosIsCertDeletable(client_4_hash_hex));
  }

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_1_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_4_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_error());
    EXPECT_EQ(delete_result->get_error(),
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR));
  }
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_4_hash_hex));
}

TEST_P(ClientCertSourceWritableUnitTest, PolicyNoneDoesNotAllowDeletion) {
  std::string client_1_hash_hex = ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "chrome");
  ASSERT_FALSE(client_1_hash_hex.empty());
  std::string client_4_hash_hex = ImportToSystemSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_4.p12"), "chrome");
  ASSERT_FALSE(client_4_hash_hex.empty());

  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kNone));

  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));
  EXPECT_FALSE(GetCertificateInfosIsCertDeletable(client_1_hash_hex));

  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_4_hash_hex));
  EXPECT_FALSE(GetCertificateInfosIsCertDeletable(client_4_hash_hex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_1_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_error());
    EXPECT_EQ(delete_result->get_error(),
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR));
  }
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_1_hash_hex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate("", client_4_hash_hex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_error());
    EXPECT_EQ(delete_result->get_error(),
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR));
  }
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(client_4_hash_hex));
}

TEST_P(ClientCertSourceWritableUnitTest, ImportPkcs12NotAllowedByPolicy) {
  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kNone));
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  ASSERT_TRUE(import_result);
  ASSERT_TRUE(import_result->is_error());
  EXPECT_EQ(import_result->get_error(), "not allowed");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_P(ClientCertSourceWritableUnitTest, ImportPkcs12PasswordWrong) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // Wrong password for the client_1.p12 file.
  fake_page_->set_mocked_import_password("wrong");

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "p12"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  ASSERT_TRUE(import_result);
  ASSERT_TRUE(import_result->is_error());
  EXPECT_EQ(import_result->get_error(),
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BAD_PASSWORD));
}

TEST_P(ClientCertSourceWritableUnitTest, ImportPkcs12PasswordEntryCancelled) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // Returning nullopt to the password entry callback signals the password
  // entry dialog was closed/cancelled without entering a password.
  fake_page_->set_mocked_import_password(std::nullopt);

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "p12"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  EXPECT_FALSE(import_result);
}

TEST_P(ClientCertSourceWritableUnitTest, ImportPkcs12FileNotFound) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      base::FilePath("non-existant-file-name"), "p12"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  ASSERT_TRUE(import_result);
  ASSERT_TRUE(import_result->is_error());
  EXPECT_EQ(import_result->get_error(),
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_READ_FILE_ERROR));
}

TEST_P(ClientCertSourceWritableUnitTest, ImportPkcs12FileSelectionCancelled) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  fake_file_select_dialog->CallFileSelectionCanceled();

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  EXPECT_FALSE(import_result);
}

TEST_P(ClientCertSourceWritableUnitTest,
       DeleteCertificateConfirmationCancelled) {
  // A certificate is required to be present for the delete dialog to display,
  // so import the test cert first.
  {
    ui::FakeSelectFileDialog::Factory* factory =
        ui::FakeSelectFileDialog::RegisterFactory();

    // The correct password for the client_1.p12 file.
    fake_page_->set_mocked_import_password("chrome");

    base::test::TestFuture<void> select_file_dialog_opened_waiter;
    factory->SetOpenCallback(
        select_file_dialog_opened_waiter.GetRepeatingCallback());

    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        import_waiter;
    DoImport(import_waiter.GetCallback());
    EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
    ui::FakeSelectFileDialog* fake_file_select_dialog =
        factory->GetLastDialog();
    ASSERT_TRUE(fake_file_select_dialog);
    ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
        net::GetTestCertsDirectory().AppendASCII("client_1.p12"), "p12"));

    certificate_manager_v2::mojom::ActionResultPtr import_result =
        import_waiter.Take();
    ASSERT_TRUE(import_result);
    EXPECT_TRUE(import_result->is_success());
  }

  std::string client_1_hash_hex =
      GetTestCertHash(net::GetTestCertsDirectory().AppendASCII("client_1.pem"));
  ASSERT_FALSE(client_1_hash_hex.empty());

  // Mock the user cancelling out of the deletion confirmation dialog.
  fake_page_->set_mocked_confirmation_result(false);
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_waiter;
  cert_source_->DeleteCertificate("", client_1_hash_hex,
                                  delete_waiter.GetCallback());

  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_waiter.Take();
  // A cancelled action should be signalled with an empty ActionResult.
  EXPECT_FALSE(delete_result);
}

TEST_P(ClientCertSourceWritableUnitTest, DeleteCertificateNotFound) {
  fake_page_->set_mocked_confirmation_result(true);

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_waiter;
  cert_source_->DeleteCertificate(
      "", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      delete_waiter.GetCallback());

  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_waiter.Take();
  ASSERT_TRUE(delete_result);
  ASSERT_TRUE(delete_result->is_error());
  EXPECT_EQ(delete_result->get_error(), "cert not found");
}

INSTANTIATE_TEST_SUITE_P(Foo,
                         ClientCertSourceWritableUnitTest,
#if BUILDFLAG(IS_CHROMEOS)
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool())
#else
                         testing::Values(true)
#endif
);
