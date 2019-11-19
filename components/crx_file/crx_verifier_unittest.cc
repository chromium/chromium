// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_verifier.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath TestFile(const std::string& file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("crx_file")
      .AppendASCII(file);
}

constexpr char kOjjHash[] = "ojjgnpkioondelmggbekfhllhdaimnho";
constexpr char kOjjKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA230uN7vYDEhdDlb4/"
    "+pg2pfL8p0FFzCF/O146NB3D5dPKuLbnNphn0OUzOrDzR/Z1XLVDlDyiA6xnb+qeRp7H8n7Wk/"
    "/gvVDNArZyForlVqWdaHLhl4dyZoNJwPKsggf30p/"
    "MxCbNfy2rzFujzn2nguOrJKzWvNt0BFqssrBpzOQl69blBezE2ZYGOnYW8mPgQV29ekIgOfJk2"
    "GgXoJBQQRRsjoPmUY7GDuEKudEB/"
    "CmWh3+"
    "mCsHBHFWbqtGhSN4YCAw3DYQzwdTcIVaIA8f2Uo4AZ4INKkrEPRL8o9mZDYtO2YHIQg8pMSRMa"
    "6AawBNYi9tZScnmgl5L1qE6z5oIwIDAQAB";

constexpr char kJlnHash[] = "jlnmailbicnpnbggmhfebbomaddckncf";
constexpr char kJlnKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtYd4M8wBjlPsc/wxS1/uXKMD6GtI7/"
    "0GLxRe6UXTYtDk91u/"
    "FdJRK9jHws+"
    "FO6Yi3XcJGtMvuojiB1j4bdiYBfvRgfTD4b7krWtWM2udKPBtHI9ikAT5aom5Bda8rCPNyaqXC"
    "6Ax+KTgQpeeJglYu7TTd/"
    "AePyvlRHtCKNkcvRQLY0b6hccALqoTzyTueDX12c8Htg76syEPbz7hSIPPfq6KEGvuVSxWAejy"
    "/y6EhwAdXRLpegul9KmL94OY1G6dpycUKwyKeXOcB6Qj5iKNcOqJAaSLxoOZby4G3cI1BcQpp/"
    "3vYccJ4qouDMfaanLe8CvFlLp4VOn833aJ8PYpLQIDAQAB";

}  // namespace

namespace crx_file {

using CrxVerifierTest = testing::Test;

TEST_F(CrxVerifierTest, ValidFullCrx3) {
  const std::vector<std::vector<uint8_t>> keys;
  const std::vector<uint8_t> hash;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";

  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);
}

TEST_F(CrxVerifierTest, Crx3RejectsCrx2) {
  const std::vector<std::vector<uint8_t>> keys;
  const std::vector<uint8_t> hash;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";

  EXPECT_EQ(VerifierResult::ERROR_HEADER_INVALID,
            Verify(TestFile("valid.crx2"), VerifierFormat::CRX3, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, VerifiesFileHash) {
  const std::vector<std::vector<uint8_t>> keys;
  std::vector<uint8_t> hash;
  EXPECT_TRUE(base::HexStringToBytes(
      "d033c510f9e4ee081ccb60ea2bf530dc2e5cb0e71085b55503c8b13b74515fe4",
      &hash));
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";

  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  hash.clear();
  EXPECT_TRUE(base::HexStringToBytes(std::string(32, '0'), &hash));
  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_EXPECTED_HASH_INVALID,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);

  hash.clear();
  EXPECT_TRUE(base::HexStringToBytes(std::string(64, '0'), &hash));
  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_FILE_HASH_FAILED,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, ChecksRequiredKeyHashes) {
  const std::vector<uint8_t> hash;

  std::vector<uint8_t> good_key;
  EXPECT_TRUE(base::HexStringToBytes(
      "e996dfa8eed34bc6614a57bb7308cd7e519bcc690841e1969f7cb173ef16800a",
      &good_key));
  const std::vector<std::vector<uint8_t>> good_keys = {good_key};
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   good_keys, hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  std::vector<uint8_t> bad_key;
  EXPECT_TRUE(base::HexStringToBytes(std::string(64, '0'), &bad_key));
  const std::vector<std::vector<uint8_t>> bad_keys = {bad_key};
  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   bad_keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, ChecksPinnedKey) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("valid_test_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("valid_no_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, ChecksPinnedKeyAcceptsTest) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_test_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ(std::string(kJlnHash), crx_id);
  EXPECT_EQ(std::string(kJlnKey), public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("valid_no_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, NullptrSafe) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   nullptr, nullptr));
}

TEST_F(CrxVerifierTest, RequiresDeveloperKey) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("unsigned.crx3"), VerifierFormat::CRX3, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

}  // namespace crx_file
