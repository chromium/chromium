// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/patcher.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/unzipper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback {
 public:
  TestCallback();
  virtual ~TestCallback() {}
  void Set(update_client::UnpackerError error, int extra_code);

  update_client::UnpackerError error_;
  int extra_code_;
  bool called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

TestCallback::TestCallback()
    : error_(update_client::UnpackerError::kNone),
      extra_code_(-1),
      called_(false) {}

void TestCallback::Set(update_client::UnpackerError error, int extra_code) {
  error_ = error;
  extra_code_ = extra_code;
  called_ = true;
}

base::FilePath test_file(const char* file) {
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

class ComponentUnpackerTest : public testing::Test {
 public:
  ComponentUnpackerTest();
  ~ComponentUnpackerTest() override;

  void UnpackComplete(const ComponentUnpacker::Result& result);

 protected:
  void RunThreads();

  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_ =
      base::ThreadTaskRunnerHandle::Get();
  base::RunLoop runloop_;
  base::OnceClosure quit_closure_ = runloop_.QuitClosure();

  ComponentUnpacker::Result result_;
};

ComponentUnpackerTest::ComponentUnpackerTest() = default;

ComponentUnpackerTest::~ComponentUnpackerTest() = default;

void ComponentUnpackerTest::RunThreads() {
  runloop_.Run();
}

void ComponentUnpackerTest::UnpackComplete(
    const ComponentUnpacker::Result& result) {
  result_ = result;
  main_thread_task_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
}

TEST_F(ComponentUnpackerTest, UnpackFullCrx) {
  auto config = base::MakeRefCounted<TestConfigurator>();
  scoped_refptr<ComponentUnpacker> component_unpacker =
      base::MakeRefCounted<ComponentUnpacker>(
          std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr,
          config->GetUnzipperFactory()->Create(),
          config->GetPatcherFactory()->Create(),
          crx_file::VerifierFormat::CRX3);
  component_unpacker->Unpack(base::BindOnce(
      &ComponentUnpackerTest::UnpackComplete, base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(UnpackerError::kNone, result_.error);
  EXPECT_EQ(0, result_.extended_error);

  base::FilePath unpack_path = result_.unpack_path;
  EXPECT_FALSE(unpack_path.empty());
  EXPECT_TRUE(base::DirectoryExists(unpack_path));
  EXPECT_EQ(jebg_public_key, result_.public_key);

  int64_t file_size = 0;
  EXPECT_TRUE(
      base::GetFileSize(unpack_path.AppendASCII("component1.dll"), &file_size));
  EXPECT_EQ(1024, file_size);
  EXPECT_TRUE(
      base::GetFileSize(unpack_path.AppendASCII("manifest.json"), &file_size));
  EXPECT_EQ(169, file_size);

  EXPECT_TRUE(base::DeleteFileRecursively(unpack_path));
}

TEST_F(ComponentUnpackerTest, UnpackFileNotFound) {
  scoped_refptr<ComponentUnpacker> component_unpacker =
      base::MakeRefCounted<ComponentUnpacker>(
          std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
          test_file("file-not-found.crx"), nullptr, nullptr, nullptr,
          crx_file::VerifierFormat::CRX3);
  component_unpacker->Unpack(base::BindOnce(
      &ComponentUnpackerTest::UnpackComplete, base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(UnpackerError::kInvalidFile, result_.error);
  EXPECT_EQ(static_cast<int>(crx_file::VerifierResult::ERROR_FILE_NOT_READABLE),
            result_.extended_error);

  EXPECT_TRUE(result_.unpack_path.empty());
}

// Tests a mismatch between the public key hash and the id of the component.
TEST_F(ComponentUnpackerTest, UnpackFileHashMismatch) {
  scoped_refptr<ComponentUnpacker> component_unpacker =
      base::MakeRefCounted<ComponentUnpacker>(
          std::vector<uint8_t>(std::begin(abag_hash), std::end(abag_hash)),
          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr, nullptr,
          nullptr, crx_file::VerifierFormat::CRX3);
  component_unpacker->Unpack(base::BindOnce(
      &ComponentUnpackerTest::UnpackComplete, base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(UnpackerError::kInvalidFile, result_.error);
  EXPECT_EQ(
      static_cast<int>(crx_file::VerifierResult::ERROR_REQUIRED_PROOF_MISSING),
      result_.extended_error);

  EXPECT_TRUE(result_.unpack_path.empty());
}

}  // namespace update_client
