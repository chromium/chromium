// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/avatar_fetcher.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace data_sharing {
namespace {
using testing::_;

ACTION_P(PostFetchReply, p0) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*arg2), p0, image_fetcher::RequestMetadata()));
}

class AvatarFetcherTest : public testing::Test {
 public:
  AvatarFetcherTest()
      : image_fetcher_(std::make_unique<image_fetcher::MockImageFetcher>()),
        avatar_fetcher_(std::make_unique<AvatarFetcher>()) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
  std::unique_ptr<AvatarFetcher> avatar_fetcher_;
};

TEST_F(AvatarFetcherTest, FetchSuccess) {
  GURL fake_avatar_url = GURL("https://www.example.com/fake_image");
  GURL fake_avatar_url_with_options =
      GURL("https://www.example.com/fake_image=s20-cc-rp-ns");

  gfx::Image expected_image(gfx::test::CreateImageSkia(20, 20));
  base::test::TestFuture<const gfx::Image&> received_image;

  EXPECT_CALL(*image_fetcher_,
              FetchImageAndData_(fake_avatar_url_with_options, _, _, _))
      .WillOnce(PostFetchReply(expected_image));
  avatar_fetcher_->Fetch(fake_avatar_url, 20, received_image.GetCallback(),
                         image_fetcher_.get());

  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, received_image.Get()));
}
}  // namespace
}  // namespace data_sharing
