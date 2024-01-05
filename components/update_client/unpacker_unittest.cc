// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unpacker.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/crx_file/crx_verifier.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class UnpackerTest : public testing::Test {
 public:
  UnpackerTest() = default;
  ~UnpackerTest() override = default;

  void UnpackComplete(const Unpacker::Result& result);

 protected:
  base::test::TaskEnvironment env_;
};

TEST_F(UnpackerTest, UnpackFullCrx) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))
          ->Create(),
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kNone);
        EXPECT_EQ(result.extended_error, 0);

        base::FilePath unpack_path = result.unpack_path;
        EXPECT_TRUE(base::DirectoryExists(unpack_path));
        EXPECT_EQ(result.public_key, jebg_public_key);

        int64_t file_size = 0;
        EXPECT_TRUE(base::GetFileSize(unpack_path.AppendASCII("component1.dll"),
                                      &file_size));
        EXPECT_EQ(file_size, 1024);
        EXPECT_TRUE(base::GetFileSize(unpack_path.AppendASCII("manifest.json"),
                                      &file_size));
        EXPECT_EQ(file_size, 169);

        EXPECT_TRUE(base::DeletePathRecursively(unpack_path));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(UnpackerTest, UnpackFileNotFound) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      GetTestFilePath("file_not_found.crx"), nullptr,
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kInvalidFile);
        EXPECT_EQ(result.extended_error,
                  static_cast<int>(
                      crx_file::VerifierResult::ERROR_FILE_NOT_READABLE));
        EXPECT_TRUE(result.unpack_path.empty());
        EXPECT_TRUE(result.public_key.empty());
        loop.Quit();
      }));
  loop.Run();
}

// Tests a mismatch between the public key hash and the id of the component.
TEST_F(UnpackerTest, UnpackFileHashMismatch) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  Unpacker::Unpack(
      std::vector<uint8_t>(std::begin(abag_hash), std::end(abag_hash)),
      GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr,
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kInvalidFile);
        EXPECT_EQ(result.extended_error,
                  static_cast<int>(
                      crx_file::VerifierResult::ERROR_REQUIRED_PROOF_MISSING));
        EXPECT_TRUE(result.unpack_path.empty());
        EXPECT_TRUE(result.public_key.empty());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(UnpackerTest, UnpackWithVerifiedContents) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  Unpacker::Unpack(
      std::vector<uint8_t>(),
      GetTestFilePath("gndmhdcefbhlchkhipcnnbkcmicncehk_22_314.crx3"),
      base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))
          ->Create(),
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting([&](const Unpacker::Result& result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(result.error, UnpackerError::kNone);
        base::FilePath unpack_path = result.unpack_path;
        EXPECT_FALSE(unpack_path.empty());
        EXPECT_TRUE(base::DirectoryExists(unpack_path));
        int64_t file_size = 0;
        EXPECT_TRUE(base::GetFileSize(
            unpack_path.AppendASCII("_metadata/verified_contents.json"),
            &file_size));
        EXPECT_EQ(file_size, 1538);
        EXPECT_TRUE(base::DeletePathRecursively(unpack_path));
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace update_client
