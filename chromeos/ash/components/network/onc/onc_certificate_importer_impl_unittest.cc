// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"

#include <cert.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/network/certificate_helper.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_type.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::onc {

class ONCCertificateImporterImplTest : public testing::Test {
 public:
  ONCCertificateImporterImplTest() = default;
  ~ONCCertificateImporterImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(public_nssdb_.is_open());
    ASSERT_TRUE(private_nssdb_.is_open());

    task_runner_ = new base::TestSimpleTaskRunner();
    thread_task_runner_handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            task_runner_);

    test_nssdb_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(public_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(private_nssdb_.slot())));

    // Test db should be empty at start of test.
    EXPECT_TRUE(ListCertsInPublicSlot().empty());
    EXPECT_TRUE(ListCertsInPrivateSlot().empty());
  }

  void TearDown() override {
    thread_task_runner_handle_.reset();
    task_runner_.reset();
  }

 protected:
  enum class ImportType { kClientCertificatesOnly, kAllCertificates };

  void OnImportCompleted(bool expected_import_success, bool success) {
    EXPECT_EQ(expected_import_success, success);
  }

  // Runs the import on the certificates specified in |filename|.
  // |import_type| specifies if only client certificates should be imported, or
  // if all certificates should be imported.
  // |expected_parse_success| should be true if at least one certificate in
  // |filename| is expected to have parse errors.
  // |expected_import_success| is the expected result of importing the
  // certificates which did not have parsing errors.
  void AddCertificatesFromFile(const std::string& filename,
                               ImportType import_type,
                               bool expected_parse_success,
                               bool expected_import_success) {
    base::Value::Dict onc =
        chromeos::onc::test_utils::ReadTestDictionary(filename);
    std::optional<base::Value> certificates_value =
        onc.Extract(::onc::toplevel_config::kCertificates);
    onc_certificates_ = std::move(*certificates_value).TakeList();

    CertificateImporterImpl importer(task_runner_, test_nssdb_.get());
    auto onc_parsed_certificates =
        std::make_unique<chromeos::onc::OncParsedCertificates>(
            onc_certificates_);
    EXPECT_EQ(expected_parse_success, !onc_parsed_certificates->has_error());
    switch (import_type) {
      case ImportType::kClientCertificatesOnly:
        importer.ImportClientCertificates(
            onc_parsed_certificates->client_certificates(),
            base::BindOnce(&ONCCertificateImporterImplTest::OnImportCompleted,
                           base::Unretained(this), expected_import_success));
        break;
      case ImportType::kAllCertificates:
        importer.ImportAllCertificatesUserInitiated(
            onc_parsed_certificates->server_or_authority_certificates(),
            onc_parsed_certificates->client_certificates(),
            base::BindOnce(&ONCCertificateImporterImplTest::OnImportCompleted,
                           base::Unretained(this), expected_import_success));
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    task_runner_->RunUntilIdle();

    public_list_ = ListCertsInPublicSlot();
    private_list_ = ListCertsInPrivateSlot();
  }

  void AddCertificateFromFile(const std::string& filename,
                              ImportType import_type,
                              net::CertType expected_type,
                              std::string* guid) {
    std::string guid_temporary;
    if (!guid)
      guid = &guid_temporary;

    AddCertificatesFromFile(filename, import_type,
                            true /* expected_parse_success */,
                            true /* expected_import_success */);

    if (expected_type == net::SERVER_CERT || expected_type == net::CA_CERT) {
      ASSERT_EQ(1u, public_list_.size());
      EXPECT_EQ(expected_type, certificate::GetCertType(public_list_[0].get()));
      EXPECT_TRUE(private_list_.empty());
    } else {  // net::USER_CERT
      EXPECT_TRUE(public_list_.empty());
      ASSERT_EQ(1u, private_list_.size());
      EXPECT_EQ(expected_type,
                certificate::GetCertType(private_list_[0].get()));
    }

    const base::Value& certificate = onc_certificates_[0];
    const std::string* guid_value =
        certificate.GetDict().FindString(::onc::certificate::kGUID);
    *guid = *guid_value;
  }

  // Certificates and the NSSCertDatabase depend on these test DBs. Destroy them
  // last.
  crypto::ScopedTestNSSDB public_nssdb_;
  crypto::ScopedTestNSSDB private_nssdb_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>
      thread_task_runner_handle_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nssdb_;
  base::Value::List onc_certificates_;
  // List of certs in the nssdb's public slot.
  net::ScopedCERTCertificateList public_list_;
  // List of certs in the nssdb's "private" slot.
  net::ScopedCERTCertificateList private_list_;

 private:
  net::ScopedCERTCertificateList ListCertsInPublicSlot() {
    return ListCertsInSlot(public_nssdb_.slot());
  }

  net::ScopedCERTCertificateList ListCertsInPrivateSlot() {
    return ListCertsInSlot(private_nssdb_.slot());
  }

  net::ScopedCERTCertificateList ListCertsInSlot(PK11SlotInfo* slot) {
    net::ScopedCERTCertificateList result;
    crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
    if (!cert_list)
      return result;
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      result.push_back(net::x509_util::DupCERTCertificate(node->cert));
    }

    std::sort(result.begin(), result.end(),
              [](const net::ScopedCERTCertificate& lhs,
                 const net::ScopedCERTCertificate& rhs) {
                return net::x509_util::CalculateFingerprint256(lhs.get()) <
                       net::x509_util::CalculateFingerprint256(rhs.get());
              });
    return result;
  }
};

TEST_F(ONCCertificateImporterImplTest, MultipleCertificates) {
  AddCertificatesFromFile("managed_toplevel2.onc", ImportType::kAllCertificates,
                          true /* expected_parse_success */,
                          true /* expected_import_success */);
  EXPECT_EQ(onc_certificates_.size(), public_list_.size());
  EXPECT_TRUE(private_list_.empty());
  EXPECT_EQ(2ul, public_list_.size());
}

TEST_F(ONCCertificateImporterImplTest, OnlyClientCertificatesImpored) {
  AddCertificatesFromFile(
      "managed_toplevel2.onc", ImportType::kClientCertificatesOnly,
      true /* expected_parse_success */, true /* expected_import_success */);
  AddCertificatesFromFile(
      "certificate-client.onc", ImportType::kClientCertificatesOnly,
      true /* expected_parse_success */, true /* expected_import_success */);
  EXPECT_EQ(0ul, public_list_.size());
  EXPECT_EQ(1ul, private_list_.size());
}

TEST_F(ONCCertificateImporterImplTest, MultipleCertificatesWithFailures) {
  AddCertificatesFromFile(
      "toplevel_partially_invalid.onc", ImportType::kAllCertificates,
      false /* expected_parse_success */, true /* expected_import_success */);
  EXPECT_EQ(3ul, onc_certificates_.size());
  EXPECT_EQ(1ul, private_list_.size());
  EXPECT_TRUE(public_list_.empty());
}

TEST_F(ONCCertificateImporterImplTest, AddClientCertificate) {
  std::string guid;
  AddCertificateFromFile("certificate-client.onc", ImportType::kAllCertificates,
                         net::USER_CERT, &guid);
  EXPECT_EQ(1ul, private_list_.size());
  EXPECT_TRUE(public_list_.empty());

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_TRUE(privkey_list);
  if (privkey_list) {
    SECKEYPrivateKeyListNode* node = PRIVKEY_LIST_HEAD(privkey_list);
    int count = 0;
    while (!PRIVKEY_LIST_END(node, privkey_list)) {
      char* name = PK11_GetPrivateKeyNickname(node->key);
      EXPECT_STREQ(guid.c_str(), name);
      PORT_Free(name);
      count++;
      node = PRIVKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPrivateKeyList(privkey_list);
  }

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_TRUE(pubkey_list);
  if (pubkey_list) {
    SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(pubkey_list);
    int count = 0;
    while (!PUBKEY_LIST_END(node, pubkey_list)) {
      count++;
      node = PUBKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPublicKeyList(pubkey_list);
  }
}

TEST_F(ONCCertificateImporterImplTest, AddServerCertificateWithWebTrust) {
  AddCertificateFromFile("certificate-server.onc", ImportType::kAllCertificates,
                         net::SERVER_CERT, NULL);

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_FALSE(pubkey_list);

  ASSERT_EQ(1u, public_list_.size());
  EXPECT_TRUE(private_list_.empty());
}

TEST_F(ONCCertificateImporterImplTest, AddWebAuthorityCertificateWithWebTrust) {
  AddCertificateFromFile("certificate-web-authority.onc",
                         ImportType::kAllCertificates, net::CA_CERT, NULL);

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_FALSE(pubkey_list);

  ASSERT_EQ(1u, public_list_.size());
  EXPECT_TRUE(private_list_.empty());
}

TEST_F(ONCCertificateImporterImplTest, AddAuthorityCertificateWithoutWebTrust) {
  AddCertificateFromFile("certificate-authority.onc",
                         ImportType::kAllCertificates, net::CA_CERT, NULL);
  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_FALSE(pubkey_list);
}

struct CertParam {
  CertParam(net::CertType certificate_type,
            const char* original_filename,
            const char* update_filename)
      : cert_type(certificate_type),
        original_file(original_filename),
        update_file(update_filename) {}

  net::CertType cert_type;
  const char* original_file;
  const char* update_file;
};

class ONCCertificateImporterImplTestWithParam :
      public ONCCertificateImporterImplTest,
      public testing::WithParamInterface<CertParam> {
};

TEST_P(ONCCertificateImporterImplTestWithParam, UpdateCertificate) {
  // First we import a certificate.
  {
    SCOPED_TRACE("Import original certificate");
    AddCertificateFromFile(GetParam().original_file,
                           ImportType::kAllCertificates, GetParam().cert_type,
                           NULL);
  }

  // Now we import the same certificate with a different GUID. In case of a
  // client cert, the cert should be retrievable via the new GUID.
  {
    SCOPED_TRACE("Import updated certificate");
    AddCertificateFromFile(GetParam().update_file, ImportType::kAllCertificates,
                           GetParam().cert_type, NULL);
  }
}

TEST_P(ONCCertificateImporterImplTestWithParam, ReimportCertificate) {
  // Verify that reimporting a client certificate works.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE("Import certificate, iteration " + base::NumberToString(i));
    AddCertificateFromFile(GetParam().original_file,
                           ImportType::kAllCertificates, GetParam().cert_type,
                           NULL);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ONCCertificateImporterImplTestWithParam,
    ONCCertificateImporterImplTestWithParam,
    ::testing::Values(CertParam(net::USER_CERT,
                                "certificate-client.onc",
                                "certificate-client-update.onc"),
                      CertParam(net::SERVER_CERT,
                                "certificate-server.onc",
                                "certificate-server-update.onc"),
                      CertParam(net::CA_CERT,
                                "certificate-web-authority.onc",
                                "certificate-web-authority-update.onc")));

}  // namespace ash::onc
