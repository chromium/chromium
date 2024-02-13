// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_source.h"

#include <atomic>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/test_data_source_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using test::TestDataSourceDelegate;

constexpr char kTestData[] = "Test Data";

class DataSourceTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
};

void CheckMimeType(const std::string& expected,
                   base::OnceClosure counter,
                   const std::string& mime_type,
                   const std::vector<uint8_t>& data) {
  EXPECT_FALSE(expected.empty());
  EXPECT_EQ(expected, mime_type);
  std::move(counter).Run();
}

void CheckTextMimeType(const std::string& expected,
                       base::OnceClosure counter,
                       const std::string& mime_type,
                       std::u16string data) {
  EXPECT_FALSE(expected.empty());
  EXPECT_EQ(expected, mime_type);
  std::move(counter).Run();
}

struct FileContents {
  std::string mime_type;
  std::string parsed_filename;
};

void CheckFileContentsMimeType(const FileContents& file_contents,
                               base::OnceClosure counter,
                               const std::string& mime_type,
                               const base::FilePath& filename,
                               const std::vector<uint8_t>& data) {
  EXPECT_FALSE(file_contents.mime_type.empty());
  EXPECT_EQ(file_contents.mime_type, mime_type);
  EXPECT_EQ(file_contents.parsed_filename, filename.value());
  std::move(counter).Run();
}

void CheckWebCustomDataMimeType(const std::string& expected,
                                base::OnceClosure counter,
                                const std::string& mime_type,
                                const std::vector<uint8_t>& data) {
  EXPECT_FALSE(mime_type.empty());
  EXPECT_EQ(expected, mime_type);
  std::move(counter).Run();
}

void IncrementFailureCounter(std::atomic_int* failure_count,
                             base::RepeatingClosure counter) {
  ++(*failure_count);
  std::move(counter).Run();
}

void CheckMimeTypesReceived(
    DataSource* data_source,
    const std::string& text_mime,
    const std::string& rtf_mime,
    const std::string& html_mime,
    const std::string& image_mime,
    const std::string& filenames_mime,
    const FileContents& file_contents,
    const std::string& web_custom_data_mime = std::string()) {
  base::RunLoop run_loop;
  base::RepeatingClosure counter =
      base::BarrierClosure(DataSource::kMaxDataTypes, run_loop.QuitClosure());
  std::atomic_int failure_count;
  failure_count.store(0);
  data_source->GetDataForPreferredMimeTypes(
      base::BindOnce(&CheckTextMimeType, text_mime, counter),
      base::BindOnce(&CheckMimeType, rtf_mime, counter),
      base::BindOnce(&CheckTextMimeType, html_mime, counter),
      base::BindOnce(&CheckMimeType, image_mime, counter),
      base::BindOnce(&CheckMimeType, filenames_mime, counter),
      base::BindOnce(&CheckFileContentsMimeType, file_contents, counter),
      base::BindOnce(&CheckWebCustomDataMimeType, web_custom_data_mime,
                     counter),
      base::BindRepeating(&IncrementFailureCounter, &failure_count, counter));
  run_loop.Run();

  int expected_failure_count = 0;
  for (const auto& mime_type :
       {text_mime, rtf_mime, html_mime, image_mime, filenames_mime,
        file_contents.mime_type, web_custom_data_mime}) {
    if (mime_type.empty())
      ++expected_failure_count;
  }
  EXPECT_EQ(expected_failure_count, failure_count.load());
}

TEST_F(DataSourceTest, ReadData) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  std::string mime_type("text/plain;charset=utf-8");
  delegate.SetData(mime_type, kTestData);
  data_source.Offer(mime_type.c_str());

  data_source.ReadDataForTesting(
      mime_type, base::BindOnce([](const std::string& mime_type,
                                   const std::vector<uint8_t>& data) {
        std::string string_data(data.begin(), data.end());
        EXPECT_EQ(std::string(kTestData), string_data);
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(DataSourceTest, ReadDataArbitraryMimeType) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  std::string mime_type("abc/def;key=value");
  delegate.SetData(mime_type, kTestData);
  data_source.Offer(mime_type.c_str());

  data_source.ReadDataForTesting(
      mime_type, base::BindOnce([](const std::string& mime_type,
                                   const std::vector<uint8_t>& data) {
        std::string string_data(data.begin(), data.end());
        EXPECT_EQ(std::string(kTestData), string_data);
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(DataSourceTest, ReadData_UnknownMimeType) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-8");

  std::string unknown_type("text/unknown");
  data_source.ReadDataForTesting(
      unknown_type, base::BindOnce([](const std::string& mime_type,
                                      const std::vector<uint8_t>& data) {
        FAIL() << "Callback should not be invoked when known "
                  "mimetype is not offerred";
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(DataSourceTest, ReadData_Destroyed) {
  TestDataSourceDelegate delegate;
  {
    DataSource data_source(&delegate);
    std::string mime_type("text/plain;charset=utf-8");
    data_source.Offer(mime_type);

    data_source.ReadDataForTesting(
        mime_type, base::BindOnce([](const std::string& mime_type,
                                     const std::vector<uint8_t>& data) {
          FAIL() << "Callback should not be invoked after "
                    "data source is destroyed";
        }));
  }
  task_environment_.RunUntilIdle();
}

TEST_F(DataSourceTest, ReadData_Cancelled) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  std::string mime_type("text/plain;charset=utf-8");
  data_source.Offer(mime_type);

  data_source.ReadDataForTesting(
      mime_type, base::BindOnce([](const std::string& mime_type,
                                   const std::vector<uint8_t>& data) {
        FAIL() << "Callback should not be invoked after cancelled";
      }));
  data_source.Cancelled();
  task_environment_.RunUntilIdle();
}

TEST_F(DataSourceTest, ReadData_Deleted) {
  TestDataSourceDelegate delegate;
  auto data_source = std::make_unique<DataSource>(&delegate);
  std::string mime_type("text/plain;charset=utf-8");
  data_source->Offer(mime_type);

  base::RunLoop run_loop;
  data_source->ReadDataForTesting(mime_type, base::DoNothing(),
                                  run_loop.QuitClosure());
  data_source.reset();
  run_loop.Run();
}

TEST_F(DataSourceTest, CheckDteMimeTypeReceived) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  const std::string kDteMimeType("chromium/x-data-transfer-endpoint");
  data_source.Offer(kDteMimeType);

  base::RunLoop run_loop;
  base::RepeatingClosure counter =
      base::BarrierClosure(1, run_loop.QuitClosure());
  std::atomic_int failure_count{0};

  data_source.ReadDataTransferEndpoint(
      base::BindOnce(&CheckTextMimeType, kDteMimeType, counter),
      base::BindRepeating(&IncrementFailureCounter, &failure_count, counter));

  run_loop.Run();
  EXPECT_EQ(0, failure_count.load());
}

TEST_F(DataSourceTest, PreferredMimeTypeUTF16) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-16");
  data_source.Offer("text/plain;charset=UTF-8");
  data_source.Offer("text/html;charset=UTF-16");
  data_source.Offer("text/html;charset=utf-8");

  CheckMimeTypesReceived(&data_source, "text/plain;charset=utf-16", "",
                         "text/html;charset=UTF-16", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeUTF16LE) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-16le");
  data_source.Offer("text/plain;charset=utf8");
  data_source.Offer("text/html;charset=utf16le");
  data_source.Offer("text/html;charset=utf-8");

  CheckMimeTypesReceived(&data_source, "text/plain;charset=utf-16le", "",
                         "text/html;charset=utf16le", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeUTF16BE) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-16be");
  data_source.Offer("text/plain;charset=UTF8");
  data_source.Offer("text/html;charset=UTF16be");
  data_source.Offer("text/html;charset=utf-8");

  CheckMimeTypesReceived(&data_source, "text/plain;charset=utf-16be", "",
                         "text/html;charset=UTF16be", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeUTFToOther) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-8");
  data_source.Offer("text/plain;charset=iso-8859-1");
  data_source.Offer("text/html;charset=utf-8");
  data_source.Offer("text/html;charset=iso-8859-1");

  CheckMimeTypesReceived(&data_source, "text/plain;charset=utf-8", "",
                         "text/html;charset=utf-8", "", "", {});
}

TEST_F(DataSourceTest, RecogniseUTF8Legaccy) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("UTF8_STRING");
  data_source.Offer("text/plain;charset=iso-8859-1");

  CheckMimeTypesReceived(&data_source, "UTF8_STRING", "", "", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeOtherToAscii) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=iso-8859-1");
  data_source.Offer("text/plain;charset=ASCII");
  data_source.Offer("text/html;charset=iso-8859-1");
  data_source.Offer("text/html;charset=ascii");

  CheckMimeTypesReceived(&data_source, "text/plain;charset=iso-8859-1", "",
                         "text/html;charset=iso-8859-1", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeOtherToUnspecified) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=iso-8859-1");
  data_source.Offer("text/plain");
  data_source.Offer("text/html;charset=iso-8859-1");
  data_source.Offer("text/html");

  CheckMimeTypesReceived(&data_source, "text/plain;charset=iso-8859-1", "",
                         "text/html;charset=iso-8859-1", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeRTF) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/rtf");

  CheckMimeTypesReceived(&data_source, "", "text/rtf", "", "", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypePNGtoBitmap) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("image/bmp");
  data_source.Offer("image/png");

  CheckMimeTypesReceived(&data_source, "", "", "", "image/png", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypePNGToJPEG) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("image/png");
  data_source.Offer("image/jpeg");
  data_source.Offer("image/jpg");

  CheckMimeTypesReceived(&data_source, "", "", "", "image/png", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeBitmaptoJPEG) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("image/bmp");
  data_source.Offer("image/jpeg");
  data_source.Offer("image/jpg");

  CheckMimeTypesReceived(&data_source, "", "", "", "image/bmp", "", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeTextUriList) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/uri-list");

  CheckMimeTypesReceived(&data_source, "", "", "", "", "text/uri-list", {});
}

TEST_F(DataSourceTest, PreferredMimeTypeOctetStream) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("application/octet-stream;name=test.jpg");

  CheckMimeTypesReceived(
      &data_source, "", "", "", "", "",
      {"application/octet-stream;name=test.jpg", "test.jpg"});
}

TEST_F(DataSourceTest, OctetStreamWithoutName) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("application/octet-stream");

  CheckMimeTypesReceived(&data_source, "", "", "", "", "", {});
}

TEST_F(DataSourceTest, OctetStreamWithQuotedName) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("application/octet-stream;name=\"t\\\\est\\\".jpg\"");

  CheckMimeTypesReceived(
      &data_source, "", "", "", "", "",
      {"application/octet-stream;name=\"t\\\\est\\\".jpg\"", "t\\est\".jpg"});
}

TEST_F(DataSourceTest, WebCustomDataMime) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  std::string web_custom_data_mime("chromium/x-web-custom-data");
  data_source.Offer(web_custom_data_mime);

  CheckMimeTypesReceived(&data_source, "", "", "", "", "", {},
                         web_custom_data_mime);
}

}  // namespace
}  // namespace exo
