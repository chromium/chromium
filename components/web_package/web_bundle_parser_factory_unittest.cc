// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser_factory.h"

#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

using testing::UnorderedElementsAreArray;

base::FilePath GetTestFilePath(const base::FilePath& path) {
  base::FilePath test_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_path);
  test_path = test_path.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));
  return test_path.Append(path);
}

}  // namespace

class WebBundleParserFactoryTest : public testing::Test {
 public:
  WebBundleParserFactoryTest()
      : factory_(std::make_unique<WebBundleParserFactory>()) {}

  std::unique_ptr<mojom::BundleDataSource> CreateFileDataSource(
      base::File file) {
    return factory_->CreateFileDataSourceForTesting(std::move(file));
  }

  void GetParserForFile(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                        base::File file,
                        const std::optional<GURL>& base_url) {
    mojom::WebBundleParserFactory* factory = factory_.get();
    mojo::PendingRemote<mojom::BundleDataSource>
        file_data_source_pending_remote;
    auto file_data_source_pending_receiver =
        file_data_source_pending_remote.InitWithNewPipeAndPassReceiver();
    factory->BindFileDataSource(std::move(file_data_source_pending_receiver),
                                std::move(file));
    return factory->GetParserForDataSource(
        std::move(receiver), base_url,
        std::move(file_data_source_pending_remote));
  }

 private:
  std::unique_ptr<WebBundleParserFactory> factory_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleParserFactoryTest, FileDataSource) {
  base::FilePath test_file =
      GetTestFilePath(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));

  base::File file(test_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  int64_t file_length = file.GetLength();
  constexpr int64_t test_length = 16;
  ASSERT_LE(test_length, file_length);
  std::vector<uint8_t> first16b(test_length);
  ASSERT_TRUE(file.ReadAndCheck(0, first16b));
  std::vector<uint8_t> last16b(test_length);
  ASSERT_TRUE(file.ReadAndCheck(file_length - test_length, last16b));

  auto data_source = CreateFileDataSource(std::move(file));

  {
    base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
    data_source->Read(/*offset=*/0, test_length, future.GetCallback());
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(first16b, *future.Get());
  }

  {
    base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
    data_source->Read(file_length - test_length, test_length,
                      future.GetCallback());
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(last16b, *future.Get());
  }

  {
    base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
    data_source->Read(file_length - test_length, test_length + 1,
                      future.GetCallback());
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(last16b, *future.Get());
  }

  {
    base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
    data_source->Read(file_length + 1, test_length, future.GetCallback());
    ASSERT_FALSE(future.Get());
  }

  {
    base::test::TestFuture<int64_t> future;
    data_source->Length(future.GetCallback());
    EXPECT_EQ(file_length, future.Get());
  }

  {
    base::test::TestFuture<bool> future;
    data_source->IsRandomAccessContext(future.GetCallback());
    EXPECT_TRUE(future.Get());
  }

  // Close the file should just work
  {
    base::test::TestFuture<void> future;
    data_source->Close(future.GetCallback());
    future.Get();
  }
}

TEST_F(WebBundleParserFactoryTest, GetParserForFile) {
  base::File file(
      GetTestFilePath(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn"))),
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  mojo::Remote<mojom::WebBundleParser> parser;
  GetParserForFile(parser.BindNewPipeAndPassReceiver(), std::move(file),
                   std::nullopt);

  mojom::BundleMetadataPtr metadata;
  {
    base::test::TestFuture<mojom::BundleMetadataPtr,
                           mojom::BundleMetadataParseErrorPtr>
        future;
    parser->ParseMetadata(/*offset=*/std::nullopt, future.GetCallback());
    metadata = std::get<0>(future.Take());
  }
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 4u);

  std::map<std::string, mojom::BundleResponsePtr> responses;
  for (const auto& item : metadata->requests) {
    base::test::TestFuture<mojom::BundleResponsePtr,
                           mojom::BundleResponseParseErrorPtr>
        future;
    parser->ParseResponse(item.second->offset, item.second->length,
                          future.GetCallback());
    auto [response, error] = future.Take();
    ASSERT_TRUE(response);
    ASSERT_FALSE(error);
    responses[item.first.spec()] = std::move(response);
  }
  ASSERT_TRUE(responses["https://test.example.org/"]);
  EXPECT_EQ(responses["https://test.example.org/"]->response_code, 200);
  EXPECT_EQ(
      responses["https://test.example.org/"]->response_headers["content-type"],
      "text/html; charset=utf-8");
  EXPECT_TRUE(responses["https://test.example.org/index.html"]);
  EXPECT_TRUE(responses["https://test.example.org/manifest.webmanifest"]);
  EXPECT_TRUE(responses["https://test.example.org/script.js"]);
}

TEST_F(WebBundleParserFactoryTest, GetParserForFileWithRelativeUrls) {
  base::File file(GetTestFilePath(base::FilePath(
                      FILE_PATH_LITERAL("mixed_absolute_relative_urls.wbn"))),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  mojo::Remote<mojom::WebBundleParser> parser;
  GetParserForFile(parser.BindNewPipeAndPassReceiver(), std::move(file),
                   GURL("https://example.com/foo/"));

  mojom::BundleMetadataPtr metadata;
  {
    base::test::TestFuture<mojom::BundleMetadataPtr,
                           mojom::BundleMetadataParseErrorPtr>
        future;
    parser->ParseMetadata(/*offset=*/std::nullopt, future.GetCallback());
    metadata = std::get<0>(future.Take());
  }
  ASSERT_TRUE(metadata);

  std::vector<GURL> requests;
  requests.reserve(metadata->requests.size());
  base::ranges::transform(metadata->requests, std::back_inserter(requests),
                          [](const auto& entry) { return entry.first; });
  EXPECT_THAT(requests, UnorderedElementsAreArray(
                            {GURL("https://test.example.org/absolute-url"),
                             GURL("https://example.com/relative-url-1"),
                             GURL("https://example.com/foo/relative-url-2")}));
}

TEST_F(WebBundleParserFactoryTest, DeleteFile) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath file_path = tmp_dir.GetPath();
  base::CreateTemporaryFile(&file_path);

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  mojo::Remote<mojom::WebBundleParser> parser;
  GetParserForFile(parser.BindNewPipeAndPassReceiver(), std::move(file),
                   std::nullopt);

  base::test::TestFuture<void> future;
  parser->Close(future.GetCallback());
  future.Get();

  EXPECT_TRUE(base::DeleteFile(file_path));
}

}  // namespace web_package
