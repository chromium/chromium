// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_source.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

constexpr char kTestData[] = "Test Data";

class DataSourceTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
};

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate() {}
  ~TestDataSourceDelegate() override {}

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* source) override {}
  void OnTarget(const base::Optional<std::string>& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    ASSERT_TRUE(
        base::WriteFileDescriptor(fd.get(), kTestData, strlen(kTestData)));
  }
  void OnCancelled() override {}
  void OnDndDropPerformed() override {}
  void OnDndFinished() override {}
  void OnAction(DndAction dnd_action) override {}
};

void CheckMimeType(const std::string& expected,
                   base::OnceClosure counter,
                   const std::string& mime_type,
                   const std::vector<uint8_t>& data) {
  EXPECT_FALSE(expected.empty());
  EXPECT_EQ(mime_type, expected);
  std::move(counter).Run();
}

void CheckTextMimeType(const std::string& expected,
                       base::OnceClosure counter,
                       const std::string& mime_type,
                       base::string16 data) {
  EXPECT_FALSE(expected.empty());
  EXPECT_EQ(mime_type, expected);
  std::move(counter).Run();
}

void IncrementCounter(base::RepeatingClosure counter) {
  std::move(counter).Run();
}

void CheckMimeTypesRecieved(DataSource* data_source,
                            const std::string& text_mime,
                            const std::string& rtf_mime,
                            const std::string& html_mime,
                            const std::string& image_mime) {
  base::RunLoop run_loop;
  base::RepeatingClosure counter =
      base::BarrierClosure(4, run_loop.QuitClosure());
  data_source->GetDataForPreferredMimeTypes(
      base::BindOnce(&CheckTextMimeType, text_mime, counter),
      base::BindOnce(&CheckMimeType, rtf_mime, counter),
      base::BindOnce(&CheckTextMimeType, html_mime, counter),
      base::BindOnce(&CheckMimeType, image_mime, counter),
      base::BindRepeating(&IncrementCounter, counter));
  run_loop.Run();
}

TEST_F(DataSourceTest, ReadData) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  std::string mime_type("text/plain;charset=utf-8");
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

TEST_F(DataSourceTest, PreferredMimeTypeUTF16) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-16");
  data_source.Offer("text/plain;charset=UTF-8");
  data_source.Offer("text/html;charset=UTF-16");
  data_source.Offer("text/html;charset=utf-8");

  CheckMimeTypesRecieved(
      &data_source,
      "text/plain;charset=utf-16",
      "",
      "text/html;charset=UTF-16",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeUTF16LE) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-16le");
  data_source.Offer("text/plain;charset=utf8");
  data_source.Offer("text/html;charset=utf16le");
  data_source.Offer("text/html;charset=utf-8");

  CheckMimeTypesRecieved(
      &data_source,
      "text/plain;charset=utf-16le",
      "",
      "text/html;charset=utf16le",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeUTF16BE) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-16be");
  data_source.Offer("text/plain;charset=UTF8");
  data_source.Offer("text/html;charset=UTF16be");
  data_source.Offer("text/html;charset=utf-8");

  CheckMimeTypesRecieved(
      &data_source,
      "text/plain;charset=utf-16be",
      "",
      "text/html;charset=UTF16be",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeUTFToOther) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=utf-8");
  data_source.Offer("text/plain;charset=iso-8859-1");
  data_source.Offer("text/html;charset=utf-8");
  data_source.Offer("text/html;charset=iso-8859-1");

  CheckMimeTypesRecieved(
      &data_source,
      "text/plain;charset=utf-8",
      "",
      "text/html;charset=utf-8",
      "");
}

TEST_F(DataSourceTest, RecogniseUTF8Legaccy) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("UTF8_STRING");
  data_source.Offer("text/plain;charset=iso-8859-1");

  CheckMimeTypesRecieved(
      &data_source,
      "UTF8_STRING",
      "",
      "",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeOtherToAscii) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=iso-8859-1");
  data_source.Offer("text/plain;charset=ASCII");
  data_source.Offer("text/html;charset=iso-8859-1");
  data_source.Offer("text/html;charset=ascii");

  CheckMimeTypesRecieved(
      &data_source,
      "text/plain;charset=iso-8859-1",
      "",
      "text/html;charset=iso-8859-1",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeOtherToUnspecified) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/plain;charset=iso-8859-1");
  data_source.Offer("text/plain");
  data_source.Offer("text/html;charset=iso-8859-1");
  data_source.Offer("text/html");

  CheckMimeTypesRecieved(
      &data_source,
      "text/plain;charset=iso-8859-1",
      "",
      "text/html;charset=iso-8859-1",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeRTF) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("text/rtf");

  CheckMimeTypesRecieved(
      &data_source,
      "",
      "text/rtf",
      "",
      "");
}

TEST_F(DataSourceTest, PreferredMimeTypeBitmapToPNG) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("image/bmp");
  data_source.Offer("image/png");

  CheckMimeTypesRecieved(
      &data_source,
      "",
      "",
      "",
      "image/bmp");
}

TEST_F(DataSourceTest, PreferredMimeTypePNGToJPEG) {
  TestDataSourceDelegate delegate;
  DataSource data_source(&delegate);
  data_source.Offer("image/png");
  data_source.Offer("image/jpeg");
  data_source.Offer("image/jpg");

  CheckMimeTypesRecieved(
      &data_source,
      "",
      "",
      "",
      "image/png");
}

}  // namespace
}  // namespace exo
