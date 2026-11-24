// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_gallery_util/safe_media_metadata_parser.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/media_gallery_util/local_media_data_source_factory.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeMediaParser : public chrome::mojom::MediaParser {
 public:
  void ParseMediaMetadata(
      const std::string& mime_type,
      int64_t total_size,
      bool get_attached_images,
      mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
      ParseMediaMetadataCallback callback) override {
    source.Bind(std::move(media_data_source));
    parse_callback_ = std::move(callback);
  }

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_FFMPEG)
  void ExtractVideoFrame(
      const std::string& mime_type,
      uint32_t total_size,
      mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
      ExtractVideoFrameCallback callback) override {
    ADD_FAILURE() << "Unexpected video frame extraction";
  }
#endif

  void GetCpuInfo(GetCpuInfoCallback callback) override {
    ADD_FAILURE() << "Unexpected CPU info request";
  }

  mojo::Remote<chrome::mojom::MediaDataSource> source;

 private:
  ParseMediaMetadataCallback parse_callback_;
};

}  // namespace

TEST(SafeMediaMetadataParserTest, FailedReadTerminatesParsing) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto file_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto factory = std::make_unique<LocalMediaDataSourceFactory>(
      temp_dir.GetPath().AppendASCII("missing-file"), file_task_runner);
  SafeMediaMetadataParser parser(/*size=*/100, "audio/mpeg",
                                 /*get_attached_images=*/false,
                                 std::move(factory));
  FakeMediaParser fake_parser;
  mojo::Receiver<chrome::mojom::MediaParser> receiver(&fake_parser);
  parser.SetMediaParserForTesting(receiver.BindNewPipeAndPassRemote());
  base::test::TestFuture<void> parser_disconnected;
  receiver.set_disconnect_handler(parser_disconnected.GetCallback());
  base::test::TestFuture<bool, chrome::mojom::MediaMetadataPtr,
                         std::unique_ptr<std::vector<metadata::AttachedImage>>>
      completion;
  parser.Start(completion.GetCallback());
  receiver.FlushForTesting();
  ASSERT_TRUE(fake_parser.source.is_bound());

  base::test::TestFuture<void> source_disconnected;
  fake_parser.source.set_disconnect_handler(source_disconnected.GetCallback());
  fake_parser.source->Read(
      0, 100, base::BindLambdaForTesting([](const std::vector<uint8_t>& data) {
        ADD_FAILURE() << "Failed read was forwarded as EOF";
      }));
  fake_parser.source.FlushForTesting();
  file_task_runner->RunPendingTasks();
  ASSERT_TRUE(completion.Wait());
  EXPECT_FALSE(completion.Get<0>());
  EXPECT_TRUE(completion.Get<1>());
  ASSERT_TRUE(completion.Get<2>());
  EXPECT_TRUE(completion.Get<2>()->empty());
  ASSERT_TRUE(source_disconnected.Wait());
  EXPECT_FALSE(fake_parser.source.is_connected());
  ASSERT_TRUE(parser_disconnected.Wait());
}
