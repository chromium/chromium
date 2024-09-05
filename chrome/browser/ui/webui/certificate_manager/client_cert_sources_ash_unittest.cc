// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

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

class ClientCertSourceAshUnitTest : public ChromeRenderViewHostTestHarness,
                                    public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatureState(
        chromeos::features::kEnablePkcs12ToChapsDualWrite, EnableDualWrite());

    ChromeRenderViewHostTestHarness::SetUp();

    auto account = AccountId::FromUserEmail("test@example.com");
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account, false, user_manager::UserType::kRegular, profile());
    fake_user_manager_->OnUserProfileCreated(account, profile()->GetPrefs());
    fake_user_manager_->LoginUser(account);

    FakeNssService::InitializeForBrowserContext(profile(),
                                                /*enable_system_slot=*/true);

    fake_page_ = std::make_unique<FakeCertificateManagerPage>(
        fake_page_remote_.BindNewPipeAndPassReceiver());

    cert_source_ =
        CreatePlatformClientCertSource(&fake_page_remote_, profile());
  }

  void TearDown() override {
    cert_source_.reset();
    fake_user_manager_.Reset();
    ui::SelectFileDialog::SetFactory(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool EnableDualWrite() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList feature_list_;

  // local_state prefs are necessary for ChromeSelectFilePolicy.
  ScopedTestingLocalState testing_local_state_{
      TestingBrowserProcess::GetGlobal()};

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote_;
  std::unique_ptr<FakeCertificateManagerPage> fake_page_;
  std::unique_ptr<CertificateManagerPageHandler::CertSource> cert_source_;
};

// Test ImportFromPKCS12 with dual-write enabled and disabled.
TEST_P(ClientCertSourceAshUnitTest, ImportPkcs12AndGetCertificateInfos) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kNssChapsDualWrittenCertsExist));

  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // TODO(crbug.com/40928765): test hardware-backed import as well.

  // The SHA256 hash of the certificate in client.p12, as a hex string.
  constexpr char kTestClientCertHashHex[] =
      "c72ab9295a0e056fc4390032fe15170a7bdc8aceb920a7254060780b3973fba7";

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

  // Software backed certs should be imported to NSS and if dual-write is
  // enabled should also be written to Chaps and should set the preference that
  // tracks whether a dual-write has occurred.
  {
    // The correct password for the client.p12 file.
    fake_page_->set_mocked_import_password("12345");

    base::test::TestFuture<void> select_file_dialog_opened_waiter;
    factory->SetOpenCallback(
        select_file_dialog_opened_waiter.GetRepeatingCallback());

    base::test::TestFuture<certificate_manager_v2::mojom::ImportResultPtr>
        import_waiter;
    cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                    import_waiter.GetCallback());
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
    EXPECT_EQ(profile()->GetPrefs()->GetBoolean(
                  prefs::kNssChapsDualWrittenCertsExist),
              EnableDualWrite());
  }

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
  cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                  import_waiter.GetCallback());
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
  cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                  import_waiter.GetCallback());
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
  cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                  import_waiter.GetCallback());
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
  cert_source_->ImportCertificate(web_contents()->GetWeakPtr(),
                                  import_waiter.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  fake_file_select_dialog->CallFileSelectionCanceled();

  certificate_manager_v2::mojom::ImportResultPtr import_result =
      import_waiter.Take();
  EXPECT_FALSE(import_result);
}

INSTANTIATE_TEST_SUITE_P(Foo, ClientCertSourceAshUnitTest, testing::Bool());
