// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/directory_key.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

TEST(PlayerCompositorDelegate, OnClick) {
  base::test::TaskEnvironment env;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  PaintPreviewBaseService service(temp_dir.GetPath(), "test", nullptr, false);
  auto file_manager = service.GetFileManager();
  auto key = file_manager->CreateKey(1U);

  GURL url("www.example.com");
  PaintPreviewProto proto;
  auto* metadata = proto.mutable_metadata();
  metadata->set_url(url.spec());
  metadata->set_version(kPaintPreviewVersion);

  GURL root_frame_link("www.chromium.org");
  auto root_frame_id = base::UnguessableToken::Create();

  auto* root_frame = proto.mutable_root_frame();
  root_frame->set_embedding_token_high(root_frame_id.GetHighForSerialization());
  root_frame->set_embedding_token_low(root_frame_id.GetLowForSerialization());
  root_frame->set_is_main_frame(true);
  auto* root_frame_link_proto = root_frame->add_links();
  root_frame_link_proto->set_url(root_frame_link.spec());
  auto* root_frame_rect_proto = root_frame_link_proto->mutable_rect();
  root_frame_rect_proto->set_x(10);
  root_frame_rect_proto->set_y(20);
  root_frame_rect_proto->set_width(30);
  root_frame_rect_proto->set_height(40);

  GURL subframe_link("www.foo.com");
  auto subframe_id = base::UnguessableToken::Create();

  auto* subframe = proto.add_subframes();
  subframe->set_embedding_token_high(subframe_id.GetHighForSerialization());
  subframe->set_embedding_token_low(subframe_id.GetLowForSerialization());
  subframe->set_is_main_frame(true);
  auto* subframe_link_proto = subframe->add_links();
  subframe_link_proto->set_url(subframe_link.spec());
  auto* subframe_rect_proto = subframe_link_proto->mutable_rect();
  subframe_rect_proto->set_x(1);
  subframe_rect_proto->set_y(2);
  subframe_rect_proto->set_width(3);
  subframe_rect_proto->set_height(4);

  base::RunLoop loop;
  file_manager->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit, scoped_refptr<FileManager> file_manager,
             const PaintPreviewProto& proto, const DirectoryKey& key) {
            file_manager->CreateOrGetDirectory(key, true);
            file_manager->SerializePaintPreviewProto(key, proto, false);
            std::move(quit).Run();
          },
          loop.QuitClosure(), file_manager, proto, key));
  loop.Run();

  {
    PlayerCompositorDelegate player_compositor_delegate(
        &service, url, key, base::DoNothing(), true);
    env.RunUntilIdle();

    auto res = player_compositor_delegate.OnClick(root_frame_id,
                                                  gfx::Rect(10, 20, 1, 1));
    ASSERT_EQ(res.size(), 1U);
    EXPECT_EQ(*(res[0]), root_frame_link);

    res = player_compositor_delegate.OnClick(root_frame_id,
                                             gfx::Rect(0, 0, 1, 1));
    EXPECT_TRUE(res.empty());

    res =
        player_compositor_delegate.OnClick(subframe_id, gfx::Rect(1, 2, 1, 1));
    ASSERT_EQ(res.size(), 1U);
    EXPECT_EQ(*(res[0]), subframe_link);
  }
  env.RunUntilIdle();
}

TEST(PlayerCompositorDelegate, CompressOnClose) {
  base::test::TaskEnvironment env;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  PaintPreviewBaseService service(temp_dir.GetPath(), "test", nullptr, false);
  auto file_manager = service.GetFileManager();
  auto key = file_manager->CreateKey(1U);
  base::FilePath dir;
  file_manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out,
             const base::Optional<base::FilePath>& file_path) {
            *out = file_path.value();
          },
          base::Unretained(&dir)));
  env.RunUntilIdle();
  std::string data = "foo";
  EXPECT_TRUE(
      base::WriteFile(dir.AppendASCII("test_file"), data.data(), data.size()));
  {
    PlayerCompositorDelegate player_compositor_delegate(
        &service, GURL(), key, base::DoNothing(), true);
    env.RunUntilIdle();
  }
  env.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(dir.AddExtensionASCII(".zip")));
}

}  // namespace paint_preview
