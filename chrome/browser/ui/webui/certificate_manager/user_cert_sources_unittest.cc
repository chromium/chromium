// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/user_cert_sources.h"

#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/net/server_certificate_database.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom-forward.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {
class FakeCertificateManagerPage
    : public certificate_manager_v2::mojom::CertificateManagerPage {
 public:
  explicit FakeCertificateManagerPage(
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPage>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  void AskForImportPassword(AskForImportPasswordCallback callback) override {
    std::move(callback).Run("");
  }

  void AskForConfirmation(const std::string& title,
                          const std::string& message,
                          AskForConfirmationCallback callback) override {
    std::move(callback).Run(confirmation_result_);
  }

  void SetConfirmationResult(bool result) { confirmation_result_ = result; }

 private:
  bool confirmation_result_;
  mojo::Receiver<certificate_manager_v2::mojom::CertificateManagerPage>
      receiver_;
};
}  // namespace

class UserCertSourcesUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  UserCertSourcesUnitTest() : state_(TestingBrowserProcess::GetGlobal()) {
    feature_list_.InitWithFeatures({features::kEnableCertManagementUIV2,
                                    features::kEnableCertManagementUIV2Write},
                                   {});
  }

  void TearDown() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AddCertToDB(scoped_refptr<net::X509Certificate> cert) {
    net::ServerCertificateDatabaseService* server_cert_service =
        net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
            profile());
    net::ServerCertificateDatabase::CertInformation cert_info;
    cert_info.sha256hash_hex = base::ToLowerASCII(base::HexEncode(
        net::X509Certificate::CalculateFingerprint256(cert->cert_buffer())
            .data));
    cert_info.der_cert = base::ToVector(cert->cert_span());
    cert_info.cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::
            CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED);
    base::test::TestFuture<bool> import_future;
    server_cert_service->AddOrUpdateUserCertificate(
        std::move(cert_info), import_future.GetCallback());
    ASSERT_TRUE(import_future.Take());
  }

  std::vector<net::ServerCertificateDatabase::CertInformation>
  GetAllCertsFromDB() {
    net::ServerCertificateDatabaseService* server_cert_service =
        net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
            profile());
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        get_certs_future;
    server_cert_service->GetAllCertificates(get_certs_future.GetCallback());
    return get_certs_future.Take();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedTestingLocalState state_;
};

TEST_F(UserCertSourcesUnitTest, TestGetCertificateInfos) {
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_1 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_1 =
      test_cert_builder_1[0]->GetX509Certificate();
  AddCertToDB(test_cert_1);
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_2 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_2 =
      test_cert_builder_2[0]->GetX509Certificate();
  AddCertToDB(test_cert_2);

  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), nullptr);
  base::test::TestFuture<
      std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr>>
      get_certs_future;
  source.GetCertificateInfos(get_certs_future.GetCallback());
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> infos =
      get_certs_future.Take();

  ASSERT_EQ(infos.size(), 2u);
  EXPECT_EQ(infos[0]->sha256hash_hex,
            base::ToLowerASCII(
                base::HexEncode(net::X509Certificate::CalculateFingerprint256(
                                    test_cert_1->cert_buffer())
                                    .data)));
  EXPECT_EQ(infos[1]->sha256hash_hex,
            base::ToLowerASCII(
                base::HexEncode(net::X509Certificate::CalculateFingerprint256(
                                    test_cert_2->cert_buffer())
                                    .data)));
}

TEST_F(UserCertSourcesUnitTest, TestImportCertificate) {
  ASSERT_EQ(GetAllCertsFromDB().size(), 0u);
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_future;
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), nullptr);
  source.ImportCertificate(web_contents()->GetWeakPtr(),
                           import_future.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("google.single.pem"), "pem"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_future.Take();
  EXPECT_TRUE(import_result->is_success());

  std::vector<net::ServerCertificateDatabase::CertInformation> certs =
      GetAllCertsFromDB();
  ASSERT_EQ(certs.size(), 1u);
  EXPECT_EQ(certs[0].sha256hash_hex,
            "f641c36cfef49bc071359ecf88eed9317b738b5989416ad401720c0a4e2e6352");
  EXPECT_EQ(
      certs[0].cert_metadata.trust().trust_type(),
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(UserCertSourcesUnitTest, TestImportCertificateNotAllowedByPref) {
  ASSERT_EQ(GetAllCertsFromDB().size(), 0u);
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_future;
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), nullptr);
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetInteger(prefs::kCACertificateManagementAllowed,
                    static_cast<int>(CACertificateManagementPermission::kNone));
  source.ImportCertificate(web_contents()->GetWeakPtr(),
                           import_future.GetCallback());
  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_future.Take();
  EXPECT_TRUE(import_result->is_error());
  EXPECT_EQ(GetAllCertsFromDB().size(), 0u);
}
#endif  //  !BUILDFLAG(IS_ANDROID)

TEST_F(UserCertSourcesUnitTest, TestImportNonExistantCertificate) {
  ASSERT_EQ(GetAllCertsFromDB().size(), 0u);
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_future;
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), nullptr);
  source.ImportCertificate(web_contents()->GetWeakPtr(),
                           import_future.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("doesnt.exist.pem"), "pem"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_future.Take();
  ASSERT_TRUE(import_result);
  EXPECT_TRUE(import_result->is_error());
}

TEST_F(UserCertSourcesUnitTest, TestCancelImportDialog) {
  ASSERT_EQ(GetAllCertsFromDB().size(), 0u);
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_future;
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), nullptr);
  source.ImportCertificate(web_contents()->GetWeakPtr(),
                           import_future.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  fake_file_select_dialog->CallFileSelectionCanceled();

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_future.Take();
  EXPECT_TRUE(import_result.is_null());
}

TEST_F(UserCertSourcesUnitTest, TestImportMultipleCertificatesFails) {
  ASSERT_EQ(GetAllCertsFromDB().size(), 0u);
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  base::test::TestFuture<void> select_file_dialog_opened_waiter;
  factory->SetOpenCallback(
      select_file_dialog_opened_waiter.GetRepeatingCallback());
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      import_future;
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), nullptr);
  source.ImportCertificate(web_contents()->GetWeakPtr(),
                           import_future.GetCallback());
  EXPECT_TRUE(select_file_dialog_opened_waiter.Wait());
  ui::FakeSelectFileDialog* fake_file_select_dialog = factory->GetLastDialog();
  ASSERT_TRUE(fake_file_select_dialog);
  ASSERT_TRUE(fake_file_select_dialog->CallFileSelected(
      net::GetTestCertsDirectory().AppendASCII("redundant-server-chain.pem"),
      "pem"));

  certificate_manager_v2::mojom::ActionResultPtr import_result =
      import_future.Take();
  ASSERT_TRUE(import_result);
  EXPECT_TRUE(import_result->is_error());
}

TEST_F(UserCertSourcesUnitTest, TestDeleteCertificate) {
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_1 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_1 =
      test_cert_builder_1[0]->GetX509Certificate();
  AddCertToDB(test_cert_1);
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_2 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_2 =
      test_cert_builder_2[0]->GetX509Certificate();
  AddCertToDB(test_cert_2);

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote;
  std::unique_ptr<FakeCertificateManagerPage> fake_page =
      std::make_unique<FakeCertificateManagerPage>(
          fake_page_remote.BindNewPipeAndPassReceiver());
  fake_page->SetConfirmationResult(true);
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), &fake_page_remote);

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_future;
  source.DeleteCertificate("",
                           base::ToLowerASCII(base::HexEncode(
                               net::X509Certificate::CalculateFingerprint256(
                                   test_cert_1->cert_buffer())
                                   .data)),
                           delete_future.GetCallback());
  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_future.Take();
  ASSERT_TRUE(delete_result);

  EXPECT_TRUE(delete_result->is_success());
  std::vector<net::ServerCertificateDatabase::CertInformation> remaining_certs =
      GetAllCertsFromDB();
  ASSERT_EQ(remaining_certs.size(), 1u);
  EXPECT_EQ(remaining_certs[0].sha256hash_hex,
            base::ToLowerASCII(
                base::HexEncode(net::X509Certificate::CalculateFingerprint256(
                                    test_cert_2->cert_buffer())
                                    .data)));
}

TEST_F(UserCertSourcesUnitTest, TestDeleteCertificateConfirmationRejected) {
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_1 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_1 =
      test_cert_builder_1[0]->GetX509Certificate();
  AddCertToDB(test_cert_1);
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_2 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_2 =
      test_cert_builder_2[0]->GetX509Certificate();
  AddCertToDB(test_cert_2);

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote;
  std::unique_ptr<FakeCertificateManagerPage> fake_page =
      std::make_unique<FakeCertificateManagerPage>(
          fake_page_remote.BindNewPipeAndPassReceiver());
  fake_page->SetConfirmationResult(false);
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), &fake_page_remote);

  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_future;
  source.DeleteCertificate("",
                           base::ToLowerASCII(base::HexEncode(
                               net::X509Certificate::CalculateFingerprint256(
                                   test_cert_1->cert_buffer())
                                   .data)),
                           delete_future.GetCallback());
  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_future.Take();
  EXPECT_TRUE(delete_result.is_null());

  std::vector<net::ServerCertificateDatabase::CertInformation> remaining_certs =
      GetAllCertsFromDB();
  EXPECT_EQ(remaining_certs.size(), 2u);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(UserCertSourcesUnitTest, TestDeleteCertificateNotAllowedByPref) {
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_1 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_1 =
      test_cert_builder_1[0]->GetX509Certificate();
  AddCertToDB(test_cert_1);
  std::vector<std::unique_ptr<net::CertBuilder>> test_cert_builder_2 =
      net::CertBuilder::CreateSimpleChain(1);
  scoped_refptr<net::X509Certificate> test_cert_2 =
      test_cert_builder_2[0]->GetX509Certificate();
  AddCertToDB(test_cert_2);

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      fake_page_remote;
  std::unique_ptr<FakeCertificateManagerPage> fake_page =
      std::make_unique<FakeCertificateManagerPage>(
          fake_page_remote.BindNewPipeAndPassReceiver());
  fake_page->SetConfirmationResult(true);
  UserCertSource source(
      "",
      chrome_browser_server_certificate_database::
          CertificateTrust_CertificateTrustType_CERTIFICATE_TRUST_TYPE_TRUSTED,
      profile(), &fake_page_remote);

  PrefService* prefs = profile()->GetPrefs();
  prefs->SetInteger(prefs::kCACertificateManagementAllowed,
                    static_cast<int>(CACertificateManagementPermission::kNone));
  base::test::TestFuture<certificate_manager_v2::mojom::ActionResultPtr>
      delete_future;
  source.DeleteCertificate("",
                           base::ToLowerASCII(base::HexEncode(
                               net::X509Certificate::CalculateFingerprint256(
                                   test_cert_1->cert_buffer())
                                   .data)),
                           delete_future.GetCallback());
  certificate_manager_v2::mojom::ActionResultPtr delete_result =
      delete_future.Take();
  EXPECT_TRUE(delete_result->is_error());

  std::vector<net::ServerCertificateDatabase::CertInformation> remaining_certs =
      GetAllCertsFromDB();
  EXPECT_EQ(remaining_certs.size(), 2u);
}
#endif  //  !BUILDFLAG(IS_ANDROID)
