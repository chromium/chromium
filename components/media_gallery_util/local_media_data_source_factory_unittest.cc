// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_gallery_util/local_media_data_source_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class LocalMediaDataSourceFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().AppendASCII("media");
    ASSERT_TRUE(base::WriteFile(path_, "media contents"));
  }

  std::unique_ptr<chrome::mojom::MediaDataSource> CreateSource(
      SafeMediaMetadataParser::MediaDataSourceFactory::MediaDataCallback
          callback) {
    LocalMediaDataSourceFactory factory(path_, file_task_runner_);
    return factory.CreateMediaDataSource(remote_.BindNewPipeAndPassReceiver(),
                                         std::move(callback));
  }

  void FinishRead() {
    file_task_runner_->RunPendingTasks();
    // The file task posts its reply before this sentinel, including when the
    // source has been destroyed and its weak callback will be dropped.
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;
  scoped_refptr<base::TestSimpleTaskRunner> file_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  mojo::Remote<chrome::mojom::MediaDataSource> remote_;
};

TEST_F(LocalMediaDataSourceFactoryTest, ReportsSuccessfulReadAndEndOfFile) {
  int callback_count = 0;
  auto source = CreateSource(base::BindLambdaForTesting(
      [&](chrome::mojom::MediaDataSource::ReadCallback callback, bool success,
          std::string data) {
        EXPECT_TRUE(success);
        EXPECT_EQ(data, callback_count == 0 ? "media contents" : "");
        ++callback_count;
        std::move(callback).Run(std::vector<uint8_t>(data.begin(), data.end()));
      }));
  source->Read(/*position=*/0, /*length=*/100, base::DoNothing());
  FinishRead();
  EXPECT_EQ(callback_count, 1);
  source->Read(/*position=*/100, /*length=*/100, base::DoNothing());
  FinishRead();
  EXPECT_EQ(callback_count, 2);
}

TEST_F(LocalMediaDataSourceFactoryTest, ReadFailureCanDestroySource) {
  int callback_count = 0;
  std::unique_ptr<chrome::mojom::MediaDataSource> source;
  source = CreateSource(base::BindLambdaForTesting(
      [&](chrome::mojom::MediaDataSource::ReadCallback callback, bool success,
          std::string data) {
        EXPECT_FALSE(success);
        EXPECT_TRUE(data.empty());
        ++callback_count;
        source.reset();
      }));
  ASSERT_TRUE(base::DeleteFile(path_));
  source->Read(/*position=*/0, /*length=*/100, base::DoNothing());
  FinishRead();
  EXPECT_EQ(callback_count, 1);
  EXPECT_FALSE(source);
}

TEST_F(LocalMediaDataSourceFactoryTest, DestroyedSourceDropsPendingRead) {
  auto source = CreateSource(base::BindLambdaForTesting(
      [](chrome::mojom::MediaDataSource::ReadCallback callback, bool success,
         std::string data) { ADD_FAILURE() << "Read outlived its source"; }));
  source->Read(/*position=*/0, /*length=*/100, base::DoNothing());
  source.reset();
  FinishRead();
}

}  // namespace
