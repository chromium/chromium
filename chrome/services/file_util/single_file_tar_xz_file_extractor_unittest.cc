// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/single_file_tar_xz_file_extractor.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/public/mojom/single_file_tar_xz_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/254591810): Add tests for SingleFileTarXzFileExtractor::ExtractChunk
// function.

namespace chrome {

using ::testing::StrictMock;

class SingleFileTarXzFileExtractorTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  SingleFileTarXzFileExtractorTest() = default;
  ~SingleFileTarXzFileExtractorTest() override = default;

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("image_writer_private").AppendASCII(file_name);
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

  // BindNewPipeAndPassRemote() requires a sequenced context.
  base::test::TaskEnvironment task_environment_;

 private:
  base::ScopedTempDir temp_dir_;
};

// TODO(b/254591810): Make MockSingleFileTarXzFileExtractorListener a mock.
class MockSingleFileTarXzFileExtractorListener
    : public chrome::mojom::SingleFileTarXzFileExtractorListener {
  // chrome::mojom::SingleFileTarXzFileExtractorListener implementation.
  void OnProgress(uint64_t total_bytes, uint64_t progress_bytes) override {}
};

TEST_F(SingleFileTarXzFileExtractorTest, Extract) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("test.tar.xz"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::FilePath out_path = temp_dir().AppendASCII("Extract_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  auto mock_listener =
      std::make_unique<MockSingleFileTarXzFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileTarXzFileExtractorListener> listener{
      mock_listener.get()};

  StrictMock<base::MockCallback<SingleFileTarXzFileExtractor::ExtractCallback>>
      callback;

  chrome::file_util::mojom::ExtractionResult result;
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&result));
  SingleFileTarXzFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), callback.Get());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, result);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ("foo\n", contents);
}

TEST_F(SingleFileTarXzFileExtractorTest, ExtractNonExistentTarXz) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("non_existent.tar.xz"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::FilePath out_path =
      temp_dir().AppendASCII("ExtractNonExistentTarXz_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  auto mock_listener =
      std::make_unique<MockSingleFileTarXzFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileTarXzFileExtractorListener> listener{
      mock_listener.get()};

  StrictMock<base::MockCallback<SingleFileTarXzFileExtractor::ExtractCallback>>
      callback;

  chrome::file_util::mojom::ExtractionResult result;
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&result));
  SingleFileTarXzFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), callback.Get());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kUnzipGenericError,
            result);
}

TEST_F(SingleFileTarXzFileExtractorTest, ZeroByteFile) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("empty_file.tar.xz"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::FilePath out_path = temp_dir().AppendASCII("ZeroByteFile_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  auto mock_listener =
      std::make_unique<MockSingleFileTarXzFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileTarXzFileExtractorListener> listener{
      mock_listener.get()};

  StrictMock<base::MockCallback<SingleFileTarXzFileExtractor::ExtractCallback>>
      callback;

  chrome::file_util::mojom::ExtractionResult result;
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&result));
  SingleFileTarXzFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), callback.Get());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, result);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  ASSERT_TRUE(contents.empty());
}

TEST_F(SingleFileTarXzFileExtractorTest, ExtractBigFile) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("2MBzeros.tar.xz"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::FilePath out_path = temp_dir().AppendASCII("ExtractBigFile_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  auto mock_listener =
      std::make_unique<MockSingleFileTarXzFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileTarXzFileExtractorListener> listener{
      mock_listener.get()};

  StrictMock<base::MockCallback<SingleFileTarXzFileExtractor::ExtractCallback>>
      callback;

  chrome::file_util::mojom::ExtractionResult result;
  EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<0>(&result));
  SingleFileTarXzFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), callback.Get());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, result);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ(contents, std::string(2097152, '\0'));
}

}  // namespace chrome
