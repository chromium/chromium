// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_collection_impl.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

namespace {

base::OnceCallback<void(const std::vector<base::UnguessableToken>&)>
ExpectedIdsCallbackFactory(
    const std::vector<base::UnguessableToken>& expected_ids) {
  return base::BindOnce(
      [](const std::vector<base::UnguessableToken>& expected_compositor_ids,
         const std::vector<base::UnguessableToken>& compositor_ids) {
        EXPECT_THAT(compositor_ids, testing::UnorderedElementsAreArray(
                                        expected_compositor_ids.begin(),
                                        expected_compositor_ids.end()));
      },
      expected_ids);
}

}  // namespace

TEST(PaintPreviewCompositorCollectionTest, TestAddCompositor) {
  base::test::TaskEnvironment task_environment;

  PaintPreviewCompositorCollectionImpl collection(mojo::NullReceiver(), false,
                                                  nullptr);

  base::UnguessableToken token_1, token_2;
  ASSERT_TRUE(token_1.is_empty());
  ASSERT_TRUE(token_2.is_empty());
  auto create_cb_1 = base::BindOnce(
      [](base::UnguessableToken* out_token,
         const base::UnguessableToken& token) { *out_token = token; },
      base::Unretained(&token_1));
  auto create_cb_2 = base::BindOnce(
      [](base::UnguessableToken* out_token,
         const base::UnguessableToken& token) { *out_token = token; },
      base::Unretained(&token_2));
  {
    mojo::Remote<mojom::PaintPreviewCompositor> compositor_1;
    collection.CreateCompositor(compositor_1.BindNewPipeAndPassReceiver(),
                                std::move(create_cb_1));
    EXPECT_FALSE(token_1.is_empty());
    EXPECT_TRUE(compositor_1.is_bound());
    EXPECT_TRUE(compositor_1.is_connected());
    collection.ListCompositors(ExpectedIdsCallbackFactory({token_1}));
    {
      mojo::Remote<mojom::PaintPreviewCompositor> compositor_2;
      collection.CreateCompositor(compositor_2.BindNewPipeAndPassReceiver(),
                                  std::move(create_cb_2));
      EXPECT_FALSE(token_2.is_empty());
      EXPECT_TRUE(compositor_2.is_bound());
      EXPECT_TRUE(compositor_2.is_connected());
      collection.ListCompositors(
          ExpectedIdsCallbackFactory({token_1, token_2}));
    }
    task_environment.RunUntilIdle();
    collection.ListCompositors(ExpectedIdsCallbackFactory({token_1}));
  }
  task_environment.RunUntilIdle();

  auto expect_empty =
      base::BindOnce([](const std::vector<base::UnguessableToken>& ids) {
        EXPECT_TRUE(ids.empty());
      });
  collection.ListCompositors(std::move(expect_empty));
}

TEST(PaintPreviewCompositorCollectionTest,
     TestCompositorRemoteOutlivesCollection) {
  base::test::TaskEnvironment task_environment;

  base::UnguessableToken token;
  auto create_cb = base::BindOnce(
      [](base::UnguessableToken* out_token,
         const base::UnguessableToken& token) { *out_token = token; },
      base::Unretained(&token));
  ASSERT_TRUE(token.is_empty());
  mojo::Remote<mojom::PaintPreviewCompositor> compositor;
  {
    PaintPreviewCompositorCollectionImpl collection(mojo::NullReceiver(), false,
                                                    nullptr);
    collection.CreateCompositor(compositor.BindNewPipeAndPassReceiver(),
                                std::move(create_cb));
    EXPECT_FALSE(token.is_empty());
    EXPECT_TRUE(compositor.is_bound());
    EXPECT_TRUE(compositor.is_connected());
  }
  task_environment.RunUntilIdle();
  EXPECT_TRUE(compositor.is_bound());
  EXPECT_FALSE(compositor.is_connected());
  // Ensure this doesn't crash even if the collection is out of scope (thus all
  // the receivers are deleted).
  compositor->SetRootFrameUrl(GURL("https://www.foo.com"));
}

}  // namespace paint_preview
