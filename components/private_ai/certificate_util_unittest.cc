// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/certificate_util.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

constexpr char kTestTcaPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDljCCAn6gAwIBAgIUfCLXRTF+hvOZMRMCihlu2cmsyAgwDQYJKoZIhvcNAQEL\n"
    "BQAwYzELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcM\n"
    "DU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3Qg\n"
    "Um9vdCBDQTAeFw0yNTEwMTAxNzEyMTlaFw0zNTEwMDgxNzEyMTlaMGMxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1Nb3VudGFpbiBW\n"
    "aWV3MRAwDgYDVQQKDAdUZXN0IENBMRUwEwYDVQQDDAxUZXN0IFJvb3QgQ0EwggEi\n"
    "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDGgR+Sc7ZYhdmNrLcg/ce/QLLq\n"
    "+uULUgGPmsHreoDB84mkPtUbYcy1z4CxGtu7JeAYv5JpJlDN5z//DTy0HxQSq2c3\n"
    "3gcDbBJ0gjasw9TTZJ+R7Vv2qXqknJjoZWyU4ctVc674HVCweOV0/7E3LMsZPaSM\n"
    "53ZOhlw/37PtRSNPVJszxoleEx3dfVmlBzQohicf+p5TTyq2Qq03EmL1cja2AhJA\n"
    "RP7HnpWJQ1FetG7HZ4BYQ77MByi9Wf8cTI2QQvTP/VQAT0hyK+FnPIQXaJW/ygd7\n"
    "34adVuMy43CHt/g69+NuZRR8u3a3F/FCjG8qNGQQNRSMhfZXv/NcVZ2tAxDzAgMB\n"
    "AAGjQjBAMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFJsmC4qYqbsduR8c4xpA\n"
    "M+2OF4irMA4GA1UdDwEB/wQEAwIBBjANBgkqhkiG9w0BAQsFAAOCAQEATr+SNcNa\n"
    "ZP+LCvF4582Pi/yZG1TAgUe14SLU0AA+BtBYDxDXaWll2n3Tz/qohoU+keG3vHlx\n"
    "fkNyggmDdefiw3aT7UGpLCmp1z7I43q0w3fw0B8lvMXl1fvl7MZB1MwtcKhLs6iJ\n"
    "DzPe6HKKTsRQb6g15Hc8eM+9hiilF2Am3ajzCjAVbc0zkuKcDUMWnic4xZ8eRByz\n"
    "I7pmUcPyqg/cvdqb4okU1vMdxyKXdu+DCPSoL+6e1JpvZWjUMsHmUb6Cf7/ibaIK\n"
    "+MpM9M9PGrqAsc4+uWRL/P6XMeg53k6KhnqYCDQlZ5Gh1NNu9YhmKwB1p6lo9w4h\n"
    "sixcP/zVzEuAHw==\n"
    "-----END CERTIFICATE-----\n";

constexpr char kTestOakCtPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDzzCCAregAwIBAgIRAPn88GGllak+BLKA2eRN3RcwDQYJKoZIhvcNAQELBQAw\n"
    "YzELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1v\n"
    "dW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3QgUm9v\n"
    "dCBDQTAeFw0yNTEwMTAxNzEyMTlaFw0yNzEwMTAxNzEyMTlaMGAxCzAJBgNVBAYT\n"
    "AlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1Nb3VudGFpbiBWaWV3\n"
    "MRAwDgYDVQQKDAdUZXN0IENBMRIwEAYDVQQDDAkxMjcuMC4wLjEwggEiMA0GCSqG\n"
    "SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDgU/TzmMEUMwLIpG3+qir3lD2mbwDfO95M\n"
    "n6PqB9Ss5VsN0azg7fnFmB01LeWzSZcUhUQP3EzSZwiIAaXYp+uT0Wqh91HnhH5S\n"
    "Kn28bw7Y27amPt7c9aRolkQRhQLtRxLfuGBxlXtih2h6RFYJ1bTI8fbJRpKLaOiD\n"
    "1dWGcSPDgB6/bAHH0qS8QG3g48AuMHi9rd0lZtP1BwdW187icsUlfQzhp28AqNqr\n"
    "S1RDCWSktlI4L7fMAd0cAycDR7/f5jew7RjcUQvUdSLfUHs86zc5HJtvCHunBayM\n"
    "Q/fx2lEGs4JFPsiBc56wpc92lq+BLKwBKkpYSx2+/x+Fwife8XgLAgMBAAGjgYAw\n"
    "fjAMBgNVHRMBAf8EAjAAMB0GA1UdDgQWBBTi4KRzlZvpbv3OKcRvB4ELlr1HujAf\n"
    "BgNVHSMEGDAWgBSbJguKmKm7HbkfHOMaQDPtjheIqzAdBgNVHSUEFjAUBggrBgEF\n"
    "BQcDAQYIKwYBBQUHAwIwDwYDVR0RBAgwBocEfwAAATANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEAZnCA2uXBuGAZWC9nP+Aipl2GZKU8DvYBPlrLApNykuD5TwlqpcmM8rz8tJPu\n"
    "0NdXyIfReY4ixmRx3CPqtIf6UBiZuX8H6zPR+123A4f/+bCzH3n2BRlTsHQIRxN7\n"
    "NfoOx1sHHdxQb4vqKO48q5y5BctRhAScKh5qCBTbKldFgrAUx7VgigjP4fsIdGQ2\n"
    "0ADBBabyk6zvtpCfO/dhOyWk00iBtK2islyuRrNwOYATB4BMB9cLSXdTHBiFqNGT\n"
    "PlX7MPNj3JXsFYUXJ+SwI5GdDcQd/VrkFCJ7ic7tByEbEEkz3uGpEv1jYOypsqh0\n"
    "r+Wnaq2GThAxh+xXDozYtXYwpw==\n"
    "-----END CERTIFICATE-----\n";

TEST(CertificateUtilTest, PemToDerValid) {
  auto tca_der = PemToDer(kTestTcaPem);
  ASSERT_TRUE(tca_der.has_value());
  EXPECT_FALSE(tca_der->empty());

  auto oakct_der = PemToDer(kTestOakCtPem);
  ASSERT_TRUE(oakct_der.has_value());
  EXPECT_FALSE(oakct_der->empty());

  // Distinct certificates produce distinct DER bytes.
  EXPECT_NE(*tca_der, *oakct_der);
}

TEST(CertificateUtilTest, PemToDerEmpty) {
  EXPECT_FALSE(PemToDer("").has_value());
}

TEST(CertificateUtilTest, PemToDerInvalidContent) {
  EXPECT_FALSE(PemToDer("invalid pem string").has_value());
  EXPECT_FALSE(
      PemToDer(
          "-----BEGIN CERTIFICATE-----\ninvalid\n-----END CERTIFICATE-----\n")
          .has_value());
}

TEST(CertificateUtilTest, PemToDerChainSingle) {
  std::vector<std::string> chain = PemToDerChain(kTestTcaPem);
  ASSERT_EQ(chain.size(), 1u);
  EXPECT_EQ(chain[0], *PemToDer(kTestTcaPem));
}

TEST(CertificateUtilTest, PemToDerChainPreservesOrder) {
  // Leaf first, then issuer, as found in a typical PEM bundle.
  std::string bundle = std::string(kTestOakCtPem) + kTestTcaPem;
  std::vector<std::string> chain = PemToDerChain(bundle);
  ASSERT_EQ(chain.size(), 2u);
  EXPECT_EQ(chain[0], *PemToDer(kTestOakCtPem));
  EXPECT_EQ(chain[1], *PemToDer(kTestTcaPem));

  // PemToDer only returns the first block.
  EXPECT_EQ(*PemToDer(bundle), chain[0]);
}

TEST(CertificateUtilTest, PemToDerChainIgnoresNonCertificateBlocks) {
  std::string bundle =
      "-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----\n" +
      std::string(kTestTcaPem) + "some trailing text\n" + kTestOakCtPem;
  std::vector<std::string> chain = PemToDerChain(bundle);
  ASSERT_EQ(chain.size(), 2u);
  EXPECT_EQ(chain[0], *PemToDer(kTestTcaPem));
  EXPECT_EQ(chain[1], *PemToDer(kTestOakCtPem));
}

TEST(CertificateUtilTest, PemToDerChainSkipsInvalidBlocks) {
  std::string bundle =
      "-----BEGIN CERTIFICATE-----\ninvalid\n-----END CERTIFICATE-----\n" +
      std::string(kTestTcaPem);
  std::vector<std::string> chain = PemToDerChain(bundle);
  ASSERT_EQ(chain.size(), 1u);
  EXPECT_EQ(chain[0], *PemToDer(kTestTcaPem));
}

TEST(CertificateUtilTest, PemToDerChainEmptyOrInvalid) {
  EXPECT_TRUE(PemToDerChain("").empty());
  EXPECT_TRUE(PemToDerChain("invalid pem string").empty());
  EXPECT_TRUE(PemToDerChain("-----BEGIN CERTIFICATE-----\ninvalid\n-----END "
                            "CERTIFICATE-----\n")
                  .empty());
}

}  // namespace
}  // namespace private_ai
