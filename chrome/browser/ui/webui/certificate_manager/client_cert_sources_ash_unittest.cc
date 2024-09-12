// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pk11pub.h>
#include <secmodt.h>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

namespace {

constexpr char kUsername[] = "test@example.com";

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

  void set_mocked_import_password(std::optional<std::string> password) {
    password_ = std::move(password);
  }

 private:
  std::optional<std::string> password_;
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
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_, /*is_affiliated=*/false, user_manager::UserType::kRegular,
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

// Test importing from a PKCS #12 file with the combinations of dual-write
// enabled/disabled, KCER client cert store enabled/disabled, and
// hardware/software-backed private key.
TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12AndGetCertificateInfos) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kNssChapsDualWrittenCertsExist));

  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // The SHA256 hash of the certificate in client.p12, as a hex string.
  constexpr char kTestClientCertHashHex[] =
      "c72ab9295a0e056fc4390032fe15170a7bdc8aceb920a7254060780b3973fba7";

  EXPECT_FALSE(SlotContainsCertWithHash(
      crypto::GetPublicSlotForChromeOSUser(username_hash()).get(),
      kTestClientCertHashHex));

  {
    base::test::TestFuture<
        std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr>>
        get_certs_waiter;
    cert_source_->GetCertificateInfos(get_certs_waiter.GetCallback());
    const auto& certs = get_certs_waiter.Get();
    for (const auto& cert : certs) {
      EXPECT_NE(cert->sha256hash_hex, kTestClientCertHashHex);
    }
  }

  {
    // The correct password for the client.p12 file.
    fake_page_->set_mocked_import_password("12345");

    base::test::TestFuture<void> select_file_dialog_opened_waiter;
    factory->SetOpenCallback(
        select_file_dialog_opened_waiter.GetRepeatingCallback());

    base::test::TestFuture<certificate_manager_v2::mojom::ImportResultPtr>
        import_waiter;
    DoImport(import_waiter.GetCallback());
    EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
    ui::FakeSelectFileDialog* fake_file_select_dialog =
        factory->GetLastDialog();
    ASSERT_TRUE(fake_file_select_dialog);
    ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
        net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

    certificate_manager_v2::mojom::ImportResultPtr import_result =
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

  EXPECT_TRUE(SlotContainsCertWithHash(
      crypto::GetPublicSlotForChromeOSUser(username_hash()).get(),
      kTestClientCertHashHex));

  {
    base::test::TestFuture<
        std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr>>
        get_certs_waiter;
    cert_source_->GetCertificateInfos(get_certs_waiter.GetCallback());
    const auto& certs = get_certs_waiter.Get();
    bool found = false;
    for (const auto& cert : certs) {
      if (cert->sha256hash_hex == kTestClientCertHashHex) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12PasswordWrong) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // Wrong password for the client.p12 file.
  fake_page_->set_mocked_import_password("wrong");

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ImportResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

  certificate_manager_v2::mojom::ImportResultPtr import_result =
      import_waiter.Take();
  ASSERT_TRUE(import_result);
  ASSERT_TRUE(import_result->is_error());
  EXPECT_EQ(import_result->get_error(), "import failed");
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

  base::test::TestFuture<certificate_manager_v2::mojom::ImportResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("client.p12"), "p12"));

  certificate_manager_v2::mojom::ImportResultPtr import_result =
      import_waiter.Take();
  EXPECT_FALSE(import_result);
}

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12FileNotFound) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ImportResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      base::FilePath("non-existant-file-name"), "p12"));

  certificate_manager_v2::mojom::ImportResultPtr import_result =
      import_waiter.Take();
  ASSERT_TRUE(import_result);
  ASSERT_TRUE(import_result->is_error());
  EXPECT_EQ(import_result->get_error(), "error reading file");
}

TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12FileSelectionCancelled) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());

  base::test::TestFuture<certificate_manager_v2::mojom::ImportResultPtr>
      import_waiter;
  DoImport(import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  fake_file_select_dialog->CallFileSelectionCanceled();

  certificate_manager_v2::mojom::ImportResultPtr import_result =
      import_waiter.Take();
  EXPECT_FALSE(import_result);
}

INSTANTIATE_TEST_SUITE_P(Foo,
                         ClientCertSourceAshUnitTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
