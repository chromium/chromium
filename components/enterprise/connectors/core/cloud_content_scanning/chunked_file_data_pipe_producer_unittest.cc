// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/chunked_file_data_pipe_producer.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kDataChunkSize = 512 * 1024;  // 512KB

class ChunkedFileDataPipeProducerTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath CreateFile(const std::string& content) {
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII("test_file");
    if (!base::WriteFile(file_path, content)) {
      return base::FilePath();
    }
    return file_path;
  }

  void ObfuscateContentInChunks(const std::vector<uint8_t>& input,
                                std::string& output) {
    enterprise_obfuscation::DownloadObfuscator obfuscator;
    size_t offset = 0;
    while (offset < input.size()) {
      size_t chunk_size = std::min(kDataChunkSize, input.size() - offset);
      bool is_last_chunk = (offset + chunk_size == input.size());
      auto result = obfuscator.ObfuscateChunk(
          base::span(input).subspan(offset, chunk_size), is_last_chunk);
      ASSERT_TRUE(result.has_value());
      output.append(result.value().begin(), result.value().end());
      offset += chunk_size;
    }
  }

  std::pair<std::string, MojoResult> ReadProducer(
      base::File file,
      bool is_obfuscated,
      int64_t file_size,
      std::optional<enterprise_obfuscation::HeaderData> header_data) {
    ChunkedFileDataPipeProducer producer(std::move(file), is_obfuscated,
                                         file_size, std::move(header_data));

    std::string content;
    int64_t offset = 0;
    MojoResult final_result = MOJO_RESULT_OK;

    while (true) {
      base::RunLoop run_loop;
      std::vector<uint8_t> read_chunk;
      MojoResult read_result = MOJO_RESULT_UNKNOWN;
      bool callback_ran = false;

      producer.ReadNextChunk(
          offset, base::BindLambdaForTesting(
                      [&run_loop, &read_chunk, &read_result, &callback_ran](
                          std::vector<uint8_t> chunk, MojoResult result) {
                        read_chunk = std::move(chunk);
                        read_result = result;
                        callback_ran = true;
                        run_loop.Quit();
                      }));

      if (!callback_ran) {
        run_loop.Run();
      }

      if (read_result != MOJO_RESULT_OK) {
        final_result = read_result;
        break;
      }

      if (read_chunk.empty()) {
        break;  // EOF
      }

      content.append(read_chunk.begin(), read_chunk.end());
      offset += read_chunk.size();
    }

    return {content, final_result};
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

class ChunkedFileDataPipeProducerParamTest
    : public ChunkedFileDataPipeProducerTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_obfuscated() const { return GetParam(); }

  void RunTestForContent(const std::string& content) {
    std::string file_content;
    std::optional<enterprise_obfuscation::HeaderData> header_data;

    if (is_obfuscated()) {
      std::vector<uint8_t> input(content.begin(), content.end());
      ObfuscateContentInChunks(input, file_content);
    } else {
      file_content = content;
    }

    base::FilePath path = CreateFile(file_content);
    ASSERT_FALSE(path.empty());

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());

    if (is_obfuscated()) {
      auto parsed_header =
          enterprise_obfuscation::ObfuscatedFileReader::ReadHeaderData(file);
      ASSERT_TRUE(parsed_header.has_value());
      header_data = std::move(parsed_header.value());
    }

    auto result = ReadProducer(std::move(file), is_obfuscated(), content.size(),
                               std::move(header_data));

    EXPECT_EQ(MOJO_RESULT_OK, result.second);
    EXPECT_EQ(content, result.first);
  }
};

}  // namespace

TEST_P(ChunkedFileDataPipeProducerParamTest, SmallFile) {
  RunTestForContent("small file content");
}

TEST_P(ChunkedFileDataPipeProducerParamTest, LargeFile) {
  RunTestForContent(std::string(2 * kDataChunkSize + 1024, 'a'));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChunkedFileDataPipeProducerParamTest,
                         testing::Bool());

TEST_F(ChunkedFileDataPipeProducerTest, DeobfuscationErrorTest) {
  std::string obfuscated_content_str(1024, '\0');
  obfuscated_content_str.replace(enterprise_obfuscation::kHeaderSize, 4, 4,
                                 '\xFF');

  base::FilePath path = CreateFile(obfuscated_content_str);
  ASSERT_FALSE(path.empty());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  enterprise_obfuscation::HeaderData header_data;

  auto result = ReadProducer(std::move(file), /*is_obfuscated=*/true, 1000,
                             std::move(header_data));

  EXPECT_EQ(result.second, MOJO_RESULT_UNKNOWN);
}

}  // namespace enterprise_connectors
