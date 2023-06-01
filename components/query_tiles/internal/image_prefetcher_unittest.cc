// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/image_prefetcher.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/query_tiles/internal/image_loader.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/test/test_utils.h"
#include "components/query_tiles/tile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace query_tiles {
namespace {

const char kImageUrl[] = "https://www.example.com/image";
const char kOtherImageUrl[] = "https://www.example.com/image/other";
const char kInvalidUrl[] = "Not a URL..";

class MockImageLoader : public ImageLoader {
 public:
  MockImageLoader() = default;
  ~MockImageLoader() override = default;

  // ImageLoader implementation.
  MOCK_METHOD(void,
              FetchImage,
              (const GURL& url, BitmapCallback callback),
              (override));
  MOCK_METHOD(void,
              PrefetchImage,
              (const GURL& url, SuccessCallback callback),
              (override));

  // Callback stubs.
  MOCK_METHOD(void, DoneCallbackStub, (), ());
};

class ImagePrefetcherTest : public testing::Test {
 public:
  ImagePrefetcherTest() = default;
  ~ImagePrefetcherTest() override = default;

 protected:
  MockImageLoader* image_loader() { return image_loader_; }
  ImagePrefetcher* prefetcher() { return image_prefetcher_.get(); }

  void Init(ImagePrefetchMode mode) {
    auto image_loader = std::make_unique<StrictMock<MockImageLoader>>();
    image_loader_ = image_loader.get();
    image_prefetcher_ = ImagePrefetcher::Create(mode, std::move(image_loader));

    ON_CALL(*image_loader_, FetchImage(_, _))
        .WillByDefault(
            Invoke([](const GURL& url, ImageLoader::BitmapCallback callback) {
              std::move(callback).Run(SkBitmap());
            }));

    ON_CALL(*image_loader_, PrefetchImage(_, _))
        .WillByDefault(
            Invoke([](const GURL& url, ImageLoader::SuccessCallback callback) {
              std::move(callback).Run(true);
            }));
  }

  void ResetImageUrls(TileGroup* group) {
    // Make the group to have one valid URL at top level, one valid URL at
    // second level, and one invalid URL.
    test::ResetTestGroup(group);
    group->tiles[0]->image_metadatas.clear();
    group->tiles[0]->image_metadatas.emplace_back(
        ImageMetadata(GURL(kInvalidUrl)));
    group->tiles[0]->sub_tiles[0]->image_metadatas.emplace_back(
        ImageMetadata(GURL(kImageUrl)));
    group->tiles[1]->image_metadatas.emplace_back(
        ImageMetadata(GURL(kOtherImageUrl)));
  }

  void Prefetch(bool is_from_reduced_mode) {
    DCHECK(image_loader_);
    DCHECK(image_prefetcher_);
    TileGroup group;
    ResetImageUrls(&group);
    prefetcher()->Prefetch(std::move(group), is_from_reduced_mode,
                           base::BindOnce(&MockImageLoader::DoneCallbackStub,
                                          base::Unretained(image_loader_)));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ImagePrefetcher> image_prefetcher_;
  raw_ptr<MockImageLoader> image_loader_;
};

// All images should be fetched for ImagePrefetchMode::kAll.
TEST_F(ImagePrefetcherTest, PrefetchAll) {
  Init(ImagePrefetchMode::kAll);
  EXPECT_CALL(*image_loader(), DoneCallbackStub());
  EXPECT_CALL(*image_loader(), FetchImage(GURL(kImageUrl), _));
  EXPECT_CALL(*image_loader(), FetchImage(GURL(kOtherImageUrl), _));

  Prefetch(false /*is_from_reduced_mode*/);
  RunUntilIdle();
}

// Top level images should be fetched for ImagePrefetchMode::kTopLevel.
TEST_F(ImagePrefetcherTest, PrefetchTopLevel) {
  Init(ImagePrefetchMode::kTopLevel);
  EXPECT_CALL(*image_loader(), DoneCallbackStub());
  EXPECT_CALL(*image_loader(), FetchImage(GURL(kOtherImageUrl), _));

  Prefetch(false /*is_from_reduced_mode*/);
  RunUntilIdle();
}

// No image should be fetched for ImagePrefetchMode::kNone.
TEST_F(ImagePrefetcherTest, PrefetchNone) {
  Init(ImagePrefetchMode::kNone);
  EXPECT_CALL(*image_loader(), DoneCallbackStub());
  EXPECT_CALL(*image_loader(), FetchImage(_, _)).Times(0);

  Prefetch(false /*is_from_reduced_mode*/);
  RunUntilIdle();
}

// Reduced mode API is called when fetching the images.
TEST_F(ImagePrefetcherTest, PrefetchReducedMode) {
  Init(ImagePrefetchMode::kTopLevel);
  EXPECT_CALL(*image_loader(), DoneCallbackStub());
  EXPECT_CALL(*image_loader(), PrefetchImage(GURL(kOtherImageUrl), _));

  Prefetch(true /*is_from_reduced_mode*/);
  RunUntilIdle();
}

}  // namespace
}  // namespace query_tiles
