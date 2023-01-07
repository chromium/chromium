// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_matching/certificate_principal_pattern.h"

#include "base/values.h"
#include "net/cert/x509_cert_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_matching {
namespace {
constexpr char kKeyCN[] = "CN";
constexpr char kKeyL[] = "L";
constexpr char kKeyO[] = "O";
constexpr char kKeyOU[] = "OU";
}  // namespace

TEST(CertificatePrincipalPatternTest, EmptyPattern) {
  CertificatePrincipalPattern pattern;
  EXPECT_TRUE(pattern.Empty());

  EXPECT_TRUE(pattern.Matches(net::CertPrincipal()));
  {
    net::CertPrincipal principal;
    principal.common_name = "CN";
    EXPECT_TRUE(pattern.Matches(principal));
  }
}

TEST(CertificatePrincipalPatternTest, MatchingOnlyCN) {
  CertificatePrincipalPattern pattern("CN" /* common_name */, "" /* locality */,
                                      "" /* organization */,
                                      "" /* organization_unit */);
  EXPECT_FALSE(pattern.Empty());

  EXPECT_FALSE(pattern.Matches(net::CertPrincipal()));
  {
    net::CertPrincipal principal;
    principal.common_name = "CN";
    EXPECT_TRUE(pattern.Matches(principal));
  }
  {
    net::CertPrincipal principal;
    principal.common_name = "CNIsWrong";
    EXPECT_FALSE(pattern.Matches(principal));
  }
  {
    net::CertPrincipal principal;
    principal.common_name = "CN";
    principal.locality_name = "NotRelevant";
    EXPECT_TRUE(pattern.Matches(principal));
  }
}

TEST(CertificatePrincipalPatternTest, MatchingEverything) {
  CertificatePrincipalPattern pattern(
      "CN" /* common_name */, "L" /* locality */, "O" /* organization */,
      "OU" /* organization_unit */);
  EXPECT_FALSE(pattern.Empty());

  // Matches an empty CertPrincipal
  EXPECT_FALSE(pattern.Matches(net::CertPrincipal()));
  net::CertPrincipal principal;
  principal.common_name = "CN";
  EXPECT_FALSE(pattern.Matches(principal));
  principal.locality_name = "L";
  EXPECT_FALSE(pattern.Matches(principal));
  principal.organization_names.push_back("O");
  EXPECT_FALSE(pattern.Matches(principal));
  principal.organization_unit_names.push_back("OU");
  EXPECT_TRUE(pattern.Matches(principal));

  // Additional entries in the lists don't cause matching to fail.
  principal.organization_names.insert(principal.organization_names.begin(),
                                      "Front");
  principal.organization_names.push_back("Back");
  principal.organization_unit_names.insert(
      principal.organization_unit_names.begin(), "Front");
  principal.organization_unit_names.push_back("Back");
  EXPECT_TRUE(pattern.Matches(principal));
}

TEST(CertificatePrincipalPatternTest, ParseFromNullptr) {
  CertificatePrincipalPattern pattern =
      CertificatePrincipalPattern::ParseFromOptionalDict(nullptr, kKeyCN, kKeyL,
                                                         kKeyO, kKeyOU);
  EXPECT_TRUE(pattern.Empty());
}

TEST(CertificatePrincipalPatternTest, ParseFromEmptyDict) {
  base::Value::Dict dict_value;
  CertificatePrincipalPattern pattern =
      CertificatePrincipalPattern::ParseFromOptionalDict(&dict_value, kKeyCN,
                                                         kKeyL, kKeyO, kKeyOU);
  EXPECT_TRUE(pattern.Empty());
}

TEST(CertificatePrincipalPatternTest, Parse) {
  base::Value::Dict dict_value;
  dict_value.Set(kKeyCN, "ValueCN");
  dict_value.Set(kKeyL, "ValueL");
  dict_value.Set(kKeyO, "ValueO");
  dict_value.Set(kKeyOU, "ValueOU");
  CertificatePrincipalPattern pattern =
      CertificatePrincipalPattern::ParseFromOptionalDict(&dict_value, kKeyCN,
                                                         kKeyL, kKeyO, kKeyOU);
  EXPECT_FALSE(pattern.Empty());
  EXPECT_EQ("ValueCN", pattern.common_name());
  EXPECT_EQ("ValueL", pattern.locality());
  EXPECT_EQ("ValueO", pattern.organization());
  EXPECT_EQ("ValueOU", pattern.organization_unit());
}

}  // namespace certificate_matching
