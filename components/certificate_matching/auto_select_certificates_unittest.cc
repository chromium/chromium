// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_matching/auto_select_certificates.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_matching {
namespace {

using ::testing::ElementsAre;

MATCHER_P(CertEq, expected_cert, "") {
  return arg->certificate()->EqualsExcludingChain(expected_cert.get());
}

base::DictValue CreateFilterValue(const std::string& issuer,
                                  const std::string& subject) {
  base::DictValue filter;
  if (!issuer.empty()) {
    filter.Set("ISSUER", base::DictValue().Set("CN", issuer));
  }
  if (!subject.empty()) {
    filter.Set("SUBJECT", base::DictValue().Set("CN", subject));
  }
  return filter;
}

}  // namespace

class AutoSelectCertificatesTest : public testing::Test {
 protected:
  void SetUp() override {
    client_1_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    ASSERT_TRUE(client_1_);
    client_2_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem");
    ASSERT_TRUE(client_2_);
  }

  net::ClientCertIdentityList GetDefaultClientCertList() const {
    return net::FakeClientCertIdentityListFromCertificateList(
        {client_1_, client_2_});
  }

  scoped_refptr<net::X509Certificate> client_1_;
  scoped_refptr<net::X509Certificate> client_2_;
};

TEST_F(AutoSelectCertificatesTest, EmptyFiltersReturnsNoMatch) {
  base::ListValue filters;
  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;

  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_TRUE(matching_certs.empty());
  EXPECT_THAT(nonmatching_certs,
              ElementsAre(CertEq(client_1_), CertEq(client_2_)));
}

TEST_F(AutoSelectCertificatesTest, IssuerFilterSelectsMatchingCert) {
  // client_1.pem has "B CA" as its issuer; client_2.pem has "E CA".
  base::ListValue filters;
  filters.Append(CreateFilterValue("B CA", ""));

  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;
  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_THAT(matching_certs, ElementsAre(CertEq(client_1_)));
  EXPECT_THAT(nonmatching_certs, ElementsAre(CertEq(client_2_)));
}

TEST_F(AutoSelectCertificatesTest, SubjectFilterSelectsMatchingCert) {
  // client_2.pem has "Client Cert D" as its subject.
  base::ListValue filters;
  filters.Append(CreateFilterValue("", "Client Cert D"));

  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;
  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_THAT(matching_certs, ElementsAre(CertEq(client_2_)));
  EXPECT_THAT(nonmatching_certs, ElementsAre(CertEq(client_1_)));
}

TEST_F(AutoSelectCertificatesTest,
       MatchingIssuerAndSubjectSelectsMatchingCert) {
  // client_1.pem has issuer "B CA" and subject "Client Cert A".
  base::ListValue filters;
  filters.Append(CreateFilterValue("B CA", "Client Cert A"));

  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;
  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_THAT(matching_certs, ElementsAre(CertEq(client_1_)));
  EXPECT_THAT(nonmatching_certs, ElementsAre(CertEq(client_2_)));
}

TEST_F(AutoSelectCertificatesTest, MismatchedIssuerOrSubjectSelectsNothing) {
  base::ListValue filters;
  filters.Append(CreateFilterValue("Bad Issuer", "Client Cert D"));
  filters.Append(CreateFilterValue("E CA", "Bad Subject"));

  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;
  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_TRUE(matching_certs.empty());
  EXPECT_THAT(nonmatching_certs,
              ElementsAre(CertEq(client_1_), CertEq(client_2_)));
}

TEST_F(AutoSelectCertificatesTest, EmptyDictFilterMatchesAllInOrder) {
  base::ListValue filters;
  filters.Append(base::DictValue());

  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;
  AutoSelectCertificates(filters,
                         net::FakeClientCertIdentityListFromCertificateList(
                             {client_2_, client_1_}),
                         &matching_certs, &nonmatching_certs);

  EXPECT_THAT(matching_certs,
              ElementsAre(CertEq(client_2_), CertEq(client_1_)));
  EXPECT_TRUE(nonmatching_certs.empty());
}

TEST_F(AutoSelectCertificatesTest,
       MultipleFiltersMatchDistinctCertsAndClearExistingOutputs) {
  base::ListValue filters;
  filters.Append(CreateFilterValue("E CA", ""));
  filters.Append(CreateFilterValue("", "Client Cert A"));

  net::ClientCertIdentityList matching_certs = GetDefaultClientCertList();
  net::ClientCertIdentityList nonmatching_certs = GetDefaultClientCertList();
  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_THAT(matching_certs,
              ElementsAre(CertEq(client_1_), CertEq(client_2_)));
  EXPECT_TRUE(nonmatching_certs.empty());
}

TEST_F(AutoSelectCertificatesTest, NonDictFilterEntriesAreIgnored) {
  base::ListValue filters;
  filters.Append("not-a-dict");
  filters.Append(CreateFilterValue("E CA", ""));

  net::ClientCertIdentityList matching_certs;
  net::ClientCertIdentityList nonmatching_certs;
  AutoSelectCertificates(filters, GetDefaultClientCertList(), &matching_certs,
                         &nonmatching_certs);

  EXPECT_THAT(matching_certs, ElementsAre(CertEq(client_2_)));
  EXPECT_THAT(nonmatching_certs, ElementsAre(CertEq(client_1_)));
}

}  // namespace certificate_matching
