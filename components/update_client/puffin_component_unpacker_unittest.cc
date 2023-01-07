// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/puffin_component_unpacker.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/unzipper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath TestFile(const char* file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

}  // namespace

namespace update_client {

class PuffinComponentUnpackerTest : public testing::Test {
 public:
  PuffinComponentUnpackerTest() = default;
  ~PuffinComponentUnpackerTest() override = default;

  void UnpackComplete(const PuffinComponentUnpacker::Result& result);

 protected:
  base::test::TaskEnvironment env_;
};

TEST_F(PuffinComponentUnpackerTest, UnpackFullCrx) {
  auto config = base::MakeRefCounted<TestConfigurator>();
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  PuffinComponentUnpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      TestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      config->GetUnzipperFactory()->Create(), crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting(
          [&loop,
           &sequence_checker](const PuffinComponentUnpacker::Result& result) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
            EXPECT_EQ(result.error, UnpackerError::kNone);
            EXPECT_EQ(result.extended_error, 0);

            base::FilePath unpack_path = result.unpack_path;
            EXPECT_TRUE(base::DirectoryExists(unpack_path));
            EXPECT_EQ(result.public_key, jebg_public_key);

            int64_t file_size = 0;
            EXPECT_TRUE(base::GetFileSize(
                unpack_path.AppendASCII("component1.dll"), &file_size));
            EXPECT_EQ(file_size, 1024);
            EXPECT_TRUE(base::GetFileSize(
                unpack_path.AppendASCII("manifest.json"), &file_size));
            EXPECT_EQ(file_size, 169);

            EXPECT_TRUE(base::DeletePathRecursively(unpack_path));
            loop.Quit();
          }));
  loop.Run();
  DETACH_FROM_SEQUENCE(sequence_checker);
}

TEST_F(PuffinComponentUnpackerTest, UnpackFileNotFound) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  PuffinComponentUnpacker::Unpack(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      TestFile("file_not_found.crx"), nullptr, crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting(
          [&loop,
           &sequence_checker](const PuffinComponentUnpacker::Result& result) {
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
  DETACH_FROM_SEQUENCE(sequence_checker);
}

// Tests a mismatch between the public key hash and the id of the component.
TEST_F(PuffinComponentUnpackerTest, UnpackFileHashMismatch) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop loop;
  PuffinComponentUnpacker::Unpack(
      std::vector<uint8_t>(std::begin(abag_hash), std::end(abag_hash)),
      TestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr,
      crx_file::VerifierFormat::CRX3,
      base::BindLambdaForTesting(
          [&loop,
           &sequence_checker](const PuffinComponentUnpacker::Result& result) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
            EXPECT_EQ(result.error, UnpackerError::kInvalidFile);
            EXPECT_EQ(
                result.extended_error,
                static_cast<int>(
                    crx_file::VerifierResult::ERROR_REQUIRED_PROOF_MISSING));
            EXPECT_TRUE(result.unpack_path.empty());
            EXPECT_TRUE(result.public_key.empty());
            loop.Quit();
          }));
  loop.Run();
  DETACH_FROM_SEQUENCE(sequence_checker);
}

}  // namespace update_client
