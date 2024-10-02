// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pk11pub.h>
#include <secmodt.h>

#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
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
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

namespace {

constexpr char kUsername[] = "test@example.com";

// The SHA256 hash of the certificate in client.p12, as a hex string.
constexpr char kTestClientCertHashHex[] =
    "c72ab9295a0e056fc4390032fe15170a7bdc8aceb920a7254060780b3973fba7";

// The SHA256 hash of the certificate in client_with_ec_key.p12, as a hex
// string.
constexpr char kTestEcClientCertHashHex[] =
    "e3ba2f8302c0a82f933f216d999eb3e85a4d709a3166333015f09903be0e6273";

bool SlotContainsCertWithHash(PK11SlotInfo* slot, std::string_view hash_hex) {
  if (!slot) {
    return false;
  }
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
  if (!cert_list) {
    return false;
  }
  net::SHA256HashValue hash;
  if (!base::HexStringToSpan(hash_hex, hash.data)) {
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
};

}  // namespace

class ClientCertSourceAshUnitTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  void SetUp() override {
    ASSERT_TRUE(test_nss_user_.constructed_successfully());
    test_nss_user_.FinishInit();

    feature_list_.InitWithFeatureStates(
        {{chromeos::features::kEnablePkcs12ToChapsDualWrite,
          dual_write_enabled()},
         {ash::features::kUseKcerClientCertStore, kcer_enabled()}});

    ASSERT_TRUE(profile_manager_.SetUp());
    crosapi::IdleServiceAsh::DisableForTesting();
    ash::LoginState::Initialize();
    crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();

    ChromeRenderViewHostTestHarness::SetUp();

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    // is_affiliated=true is required for nss_service_chromeos to configure the
    // system slot.
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_, /*is_affiliated=*/true, user_manager::UserType::kRegular,
        profile());
    fake_user_manager_->OnUserProfileCreated(account_, profile()->GetPrefs());
    fake_user_manager_->LoginUser(account_);

    fake_page_ = std::make_unique<FakeCertificateManagerPage>(
        fake_page_remote_.BindNewPipeAndPassReceiver());

    cert_source_ =
        CreatePlatformClientCertSource(&fake_page_remote_, profile());
  }

  void TearDown() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    cert_source_.reset();
    fake_user_manager_.Reset();
    crosapi_manager_.reset();
    ash::LoginState::Shutdown();
    kcer::KcerFactoryAsh::ClearNssTokenMapForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool dual_write_enabled() const { return std::get<0>(GetParam()); }
  bool kcer_enabled() const { return std::get<1>(GetParam()); }
  bool use_hardware_backed() const { return std::get<2>(GetParam()); }

  std::string username_hash() const {
    return user_manager::FakeUserManager::GetFakeUsernameHash(account_);
  }

  void DoImport(
      CertificateManagerPageHandler::ImportCertificateCallback callback) {
    if (use_hardware_backed()) {
      cert_source_->ImportAndBindCertificate(web_contents()->GetWeakPtr(),
                                             std::move(callback));
    } else {
      cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                      std::move(callback));
    }
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

  // Imports a client certificate directly, without going through the UI or
  // checking the management allowed policy.
  testing::AssertionResult ImportForTesting(base::FilePath file_path,
                                            std::string_view password,
                                            bool import_to_system_slot) {
    base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
    NssServiceFactory::GetForContext(profile())
        ->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
    net::NSSCertDatabase* nss_db = nss_waiter.Get();

    crypto::ScopedPK11Slot slot;
    if (import_to_system_slot) {
      slot = nss_db->GetSystemSlot();
    } else if (use_hardware_backed()) {
      slot = nss_db->GetPrivateSlot();
    } else {
      slot = nss_db->GetPublicSlot();
    }

    std::string file_data;
    if (!base::ReadFileToString(file_path, &file_data)) {
      return testing::AssertionFailure()
             << "error reading " << file_path.AsUTF8Unsafe();
    }

    int r = nss_db->ImportFromPKCS12(slot.get(), std::move(file_data),
                                     base::UTF8ToUTF16(password),
                                     /*is_extractable=*/true, nullptr);
    if (r != net::OK) {
      return testing::AssertionFailure() << "NSS import result " << r;
    }

    return testing::AssertionSuccess();
  }

  testing::AssertionResult ImportToUserSlotForTesting(
      base::FilePath file_path,
      std::string_view password) {
    return ImportForTesting(file_path, password,
                            /*import_to_system_slot=*/false);
  }

  testing::AssertionResult ImportToSystemSlotForTesting(
      base::FilePath file_path,
      std::string_view password) {
    return ImportForTesting(file_path, password,
                            /*import_to_system_slot=*/true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  AccountId account_{AccountId::FromUserEmail(kUsername)};
  crypto::ScopedTestNSSChromeOSUser test_nss_user_{username_hash()};
  crypto::ScopedTestSystemNSSKeySlot test_nss_system_slot_{
      /*simulate_token_loader=*/true};

  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote_;
  std::unique_ptr<FakeCertificateManagerPage> fake_page_;
  std::unique_ptr<CertificateManagerPageHandler::CertSource> cert_source_;
};

// Test importing from a PKCS #12 file and then deleting the imported cert,
// with no policy set.
TEST_P(ClientCertSourceAshUnitTest,
       ImportPkcs12AndGetCertificateInfosAndDelete) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kNssChapsDualWrittenCertsExist));

  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  EXPECT_FALSE(SlotContainsCertWithHash(
      crypto::GetPublicSlotForChromeOSUser(username_hash()).get(),
      kTestClientCertHashHex));
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));

  {
    // The correct password for the client.p12 file.
    fake_page_->set_mocked_import_password("12345");

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
        net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

    certificate_manager_v2::mojom::ActionResultPtr import_result =
        import_waiter.Take();
    ASSERT_TRUE(import_result);
    EXPECT_TRUE(import_result->is_success());
    // The cert should be dual written only if dual-write feature is enabled
    // and the import was not hardware backed (if it's hardware backed it
    // already gets imported to Chaps so the dual write isn't needed.)
    EXPECT_EQ(profile()->GetPrefs()->GetBoolean(
                  prefs::kNssChapsDualWrittenCertsExist),
              dual_write_enabled() && !use_hardware_backed());
  }

  // The cert should be dual written only if dual-write feature is enabled
  // and the import was not hardware backed (if it's hardware backed it
  // already gets imported to Chaps so the dual write isn't needed.)
  EXPECT_EQ(
      profile()->GetPrefs()->GetBoolean(prefs::kNssChapsDualWrittenCertsExist),
      dual_write_enabled() && !use_hardware_backed());

  EXPECT_TRUE(SlotContainsCertWithHash(
      crypto::GetPublicSlotForChromeOSUser(username_hash()).get(),
      kTestClientCertHashHex));
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(kTestClientCertHashHex));

  // Now delete the imported certificate, and verify that it is no longer
  // present.
  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }

  EXPECT_FALSE(SlotContainsCertWithHash(
      crypto::GetPublicSlotForChromeOSUser(username_hash()).get(),
      kTestClientCertHashHex));
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));
}

TEST_P(ClientCertSourceAshUnitTest, PolicyAllAllowsDeletion) {
  ASSERT_TRUE(ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "12345"));
  ASSERT_TRUE(ImportToSystemSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_with_ec_key.p12"),
      "123456"));

  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kAll));

  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(kTestClientCertHashHex));

  EXPECT_TRUE(
      GetCertificateInfosContainsCertWithHash(kTestEcClientCertHashHex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(kTestEcClientCertHashHex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestEcClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }
  EXPECT_FALSE(
      GetCertificateInfosContainsCertWithHash(kTestEcClientCertHashHex));
}

TEST_P(ClientCertSourceAshUnitTest,
       PolicyUserOnlyAllowsDeletionOfUserCertsOnly) {
  ASSERT_TRUE(ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "12345"));
  ASSERT_TRUE(ImportToSystemSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_with_ec_key.p12"),
      "123456"));

  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kUserOnly));

  // A client certificate in the user slot should be deletable.
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));
  EXPECT_TRUE(GetCertificateInfosIsCertDeletable(kTestClientCertHashHex));

  // A client certificate in the system slot should not be deletable.
  EXPECT_TRUE(
      GetCertificateInfosContainsCertWithHash(kTestEcClientCertHashHex));
  if (kcer_enabled()) {
    EXPECT_FALSE(GetCertificateInfosIsCertDeletable(kTestEcClientCertHashHex));
  } else {
    // TODO(crbug.com/40928765): the delete button visibility is not set
    // properly when kcer is disabled, for system certs with
    // ClientCertificateManagementAllowed policy set to UserOnly. The policy
    // should still be enforced correctly when actually attempting to delete
    // the certificate below.
    EXPECT_TRUE(GetCertificateInfosIsCertDeletable(kTestEcClientCertHashHex));
  }

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_success());
  }
  EXPECT_FALSE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestEcClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_error());
    EXPECT_EQ(delete_result->get_error(),
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR));
  }
  EXPECT_TRUE(
      GetCertificateInfosContainsCertWithHash(kTestEcClientCertHashHex));
}

TEST_P(ClientCertSourceAshUnitTest, PolicyNoneDoesNotAllowDeletion) {
  ASSERT_TRUE(ImportToUserSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "12345"));
  ASSERT_TRUE(ImportToSystemSlotForTesting(
      net::GetTestCertsDirectory().AppendASCII("client_with_ec_key.p12"),
      "123456"));

  profile()->GetPrefs()->SetInteger(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kNone));

  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));
  EXPECT_FALSE(GetCertificateInfosIsCertDeletable(kTestClientCertHashHex));

  EXPECT_TRUE(
      GetCertificateInfosContainsCertWithHash(kTestEcClientCertHashHex));
  EXPECT_FALSE(GetCertificateInfosIsCertDeletable(kTestEcClientCertHashHex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_error());
    EXPECT_EQ(delete_result->get_error(),
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR));
  }
  EXPECT_TRUE(GetCertificateInfosContainsCertWithHash(kTestClientCertHashHex));

  {
    fake_page_->set_mocked_confirmation_result(true);
    base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
        delete_waiter;
    cert_source_->DeleteCertificate(kTestEcClientCertHashHex,
                                    delete_waiter.GetCallback());

    certificate_manager_v2::mojom::ActionResultPtr delete_result =
        delete_waiter.Take();
    ASSERT_TRUE(delete_result);
    ASSERT_TRUE(delete_result->is_error());
    EXPECT_EQ(delete_result->get_error(),
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR));
  }
  EXPECT_TRUE(
      GetCertificateInfosContainsCertWithHash(kTestEcClientCertHashHex));
}

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12NotAllowedByPolicy) {
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

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12PasswordWrong) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // Wrong password for the client.p12 file.
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
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  ASSERT_TRUE(import_result);
  ASSERT_TRUE(import_result->is_error());
  EXPECT_EQ(import_result->get_error(),
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BAD_PASSWORD));
}

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12PasswordEntryCancelled) {
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
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_waiter.Take();
  EXPECT_FALSE(import_result);
}

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12FileNotFound) {
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

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12FileSelectionCancelled) {
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

TEST_P(ClientCertSourceAshUnitTest, DeleteCertificateConfirmationCancelled) {
  // A certificate is required to be present for the delete dialog to display,
  // so import the test cert first.
  {
    ui::FakeSelectFileDialog::Factory* factory =
        ui::FakeSelectFileDialog::RegisterFactory();

    // The correct password for the client.p12 file.
    fake_page_->set_mocked_import_password("12345");

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
        net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

    certificate_manager_v2::mojom::ActionResultPtr import_result =
        import_waiter.Take();
    ASSERT_TRUE(import_result);
    EXPECT_TRUE(import_result->is_success());
  }

  // Mock the user cancelling out of the deletion confirmation dialog.
  fake_page_->set_mocked_confirmation_result(false);
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_waiter;
  cert_source_->DeleteCertificate(kTestClientCertHashHex,
                                  delete_waiter.GetCallback());

  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_waiter.Take();
  // A cancelled action should be signalled with an empty ActionResult.
  EXPECT_FALSE(delete_result);
}

TEST_P(ClientCertSourceAshUnitTest, DeleteCertificateNotFound) {
  fake_page_->set_mocked_confirmation_result(true);

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_waiter;
  cert_source_->DeleteCertificate(
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      delete_waiter.GetCallback());

  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_waiter.Take();
  ASSERT_TRUE(delete_result);
  ASSERT_TRUE(delete_result->is_error());
  EXPECT_EQ(delete_result->get_error(), "cert not found");
}

INSTANTIATE_TEST_SUITE_P(Foo,
                         ClientCertSourceAshUnitTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
