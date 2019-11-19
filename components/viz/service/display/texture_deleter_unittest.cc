// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/texture_deleter.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace viz {
namespace {

TEST(TextureDeleterTest, Destroy) {
  auto deleter =
      std::make_unique<TextureDeleter>(base::ThreadTaskRunnerHandle::Get());

  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();
  context_provider->BindToCurrentThread();

  auto* sii = context_provider->SharedImageInterface();

  gpu::Mailbox mailbox =
      sii->CreateSharedImage(ResourceFormat::RGBA_8888, gfx::Size(1, 1),
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);

  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(1u, sii->shared_image_count());

  std::unique_ptr<SingleReleaseCallback> cb =
      deleter->GetReleaseCallback(context_provider, mailbox);
  EXPECT_FALSE(context_provider->HasOneRef());
  EXPECT_EQ(1u, sii->shared_image_count());

  // When the deleter is destroyed, it immediately drops its ref on the
  // ContextProvider, and deletes the shared image.
  deleter = nullptr;
  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(0u, sii->shared_image_count());

  // Run the scoped release callback before destroying it, but it won't do
  // anything.
  cb->Run(gpu::SyncToken(), false);
}

TEST(TextureDeleterTest, NullTaskRunner) {
  auto deleter = std::make_unique<TextureDeleter>(nullptr);

  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();
  context_provider->BindToCurrentThread();

  auto* sii = context_provider->SharedImageInterface();

  gpu::Mailbox mailbox =
      sii->CreateSharedImage(ResourceFormat::RGBA_8888, gfx::Size(1, 1),
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);

  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(1u, sii->shared_image_count());

  std::unique_ptr<SingleReleaseCallback> cb =
      deleter->GetReleaseCallback(context_provider, mailbox);
  EXPECT_FALSE(context_provider->HasOneRef());
  EXPECT_EQ(1u, sii->shared_image_count());

  cb->Run(gpu::SyncToken(), false);

  // With no task runner the callback will immediately drops its ref on the
  // ContextProvider and delete the shared image.
  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(0u, sii->shared_image_count());
}

}  // namespace
}  // namespace viz
