// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser_factory.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

base::FilePath GetTestFilePath(const base::FilePath& path) {
  base::FilePath test_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
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
      mojo::PendingReceiver<mojom::BundleDataSource> receiver,
      base::File file) {
    return factory_->CreateFileDataSourceForTesting(std::move(receiver),
                                                    std::move(file));
  }

  void GetParserForFile(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                        base::File file) {
    mojom::WebBundleParserFactory* factory = factory_.get();
    return factory->GetParserForFile(std::move(receiver), std::move(file));
  }

 private:
  std::unique_ptr<WebBundleParserFactory> factory_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleParserFactoryTest, FileDataSource) {
  base::FilePath test_file =
      GetTestFilePath(base::FilePath(FILE_PATH_LITERAL("hello.wbn")));

  base::File file(test_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  int64_t file_length = file.GetLength();
  constexpr int64_t test_length = 16;
  ASSERT_LE(test_length, file_length);
  std::vector<uint8_t> first16b(test_length);
  ASSERT_EQ(test_length, file.Read(0, reinterpret_cast<char*>(first16b.data()),
                                   first16b.size()));
  std::vector<uint8_t> last16b(test_length);
  ASSERT_EQ(test_length,
            file.Read(file_length - test_length,
                      reinterpret_cast<char*>(last16b.data()), last16b.size()));

  mojo::PendingRemote<mojom::BundleDataSource> remote;
  auto data_source = CreateFileDataSource(
      remote.InitWithNewPipeAndPassReceiver(), std::move(file));

  base::Optional<std::vector<uint8_t>> result_data;
  {
    base::RunLoop run_loop;
    data_source->Read(
        /*offset=*/0, test_length,
        base::BindLambdaForTesting(
            [&result_data,
             &run_loop](const base::Optional<std::vector<uint8_t>>& data) {
              result_data = data;
              run_loop.QuitClosure().Run();
            }));
    run_loop.Run();
  }
  ASSERT_TRUE(result_data);
  EXPECT_EQ(first16b, *result_data);

  {
    base::RunLoop run_loop;
    data_source->Read(
        file_length - test_length, test_length,
        base::BindLambdaForTesting(
            [&result_data,
             &run_loop](const base::Optional<std::vector<uint8_t>>& data) {
              result_data = data;
              run_loop.QuitClosure().Run();
            }));
    run_loop.Run();
  }
  ASSERT_TRUE(result_data);
  EXPECT_EQ(last16b, *result_data);

  {
    base::RunLoop run_loop;
    data_source->Read(
        file_length - test_length, test_length + 1,
        base::BindLambdaForTesting(
            [&result_data,
             &run_loop](const base::Optional<std::vector<uint8_t>>& data) {
              result_data = data;
              run_loop.QuitClosure().Run();
            }));
    run_loop.Run();
  }
  ASSERT_TRUE(result_data);
  EXPECT_EQ(last16b, *result_data);

  {
    base::RunLoop run_loop;
    data_source->Read(
        file_length + 1, test_length,
        base::BindLambdaForTesting(
            [&result_data,
             &run_loop](const base::Optional<std::vector<uint8_t>>& data) {
              result_data = data;
              run_loop.QuitClosure().Run();
            }));
    run_loop.Run();
  }
  ASSERT_FALSE(result_data);
}

TEST_F(WebBundleParserFactoryTest, GetParserForFile) {
  base::File file(
      GetTestFilePath(base::FilePath(FILE_PATH_LITERAL("hello.wbn"))),
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  mojo::Remote<mojom::WebBundleParser> parser;
  GetParserForFile(parser.BindNewPipeAndPassReceiver(), std::move(file));

  mojom::BundleMetadataPtr metadata;
  {
    base::RunLoop run_loop;
    parser->ParseMetadata(base::BindLambdaForTesting(
        [&metadata, &run_loop](mojom::BundleMetadataPtr parsed_metadata,
                               mojom::BundleMetadataParseErrorPtr error) {
          metadata = std::move(parsed_metadata);
          run_loop.QuitClosure().Run();
        }));
    run_loop.Run();
  }
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 4u);

  std::map<std::string, mojom::BundleResponsePtr> responses;
  for (const auto& item : metadata->requests) {
    ASSERT_TRUE(item.second->variants_value.empty());
    ASSERT_EQ(item.second->response_locations.size(), 1u);
    base::RunLoop run_loop;
    parser->ParseResponse(item.second->response_locations[0]->offset,
                          item.second->response_locations[0]->length,
                          base::BindLambdaForTesting(
                              [&item, &responses, &run_loop](
                                  mojom::BundleResponsePtr response,
                                  mojom::BundleResponseParseErrorPtr error) {
                                ASSERT_TRUE(response);
                                ASSERT_FALSE(error);
                                responses[item.first.spec()] =
                                    std::move(response);
                                run_loop.QuitClosure().Run();
                              }));
    run_loop.Run();
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

}  // namespace web_package
