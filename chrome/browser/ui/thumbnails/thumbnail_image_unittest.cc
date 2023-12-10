// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr int kTestBitmapWidth = 200;
constexpr int kTestBitmapHeight = 123;

class CallbackWaiter {
 public:
  CallbackWaiter() {
    callback_ = base::BindRepeating(&CallbackWaiter::HandleCallback,
                                    weak_ptr_factory_.GetWeakPtr());
  }

  base::RepeatingClosure callback() { return callback_; }

  bool called() const { return called_; }

  void Reset() { called_ = false; }

  void Wait() {
    if (called_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void HandleCallback() {
    if (quit_closure_)
      std::move(quit_closure_).Run();
    called_ = true;
  }

  base::RepeatingClosure callback_;
  base::OnceClosure quit_closure_;
  bool called_ = false;

  base::WeakPtrFactory<CallbackWaiter> weak_ptr_factory_{this};
};

class StubDelegate : public ThumbnailImage::Delegate {
 public:
  StubDelegate() = default;
  ~StubDelegate() override = default;

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {}
};

}  // anonymous namespace

class ThumbnailImageTest : public testing::Test,
                           public ThumbnailImage::Delegate {
 public:
  ThumbnailImageTest() = default;

  ThumbnailImageTest(const ThumbnailImageTest&) = delete;
  ThumbnailImageTest& operator=(const ThumbnailImageTest&) = delete;

 protected:
  std::vector<uint8_t> Compress(SkBitmap bitmap) const {
    return ThumbnailImage::CompressBitmap(bitmap, std::nullopt);
  }

  bool is_being_observed() const { return is_being_observed_; }

 private:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {
    is_being_observed_ = is_being_observed;
  }

  bool is_being_observed_ = false;
  base::test::TaskEnvironment task_environment_;
};

using Subscription = ThumbnailImage::Subscription;

TEST_F(ThumbnailImageTest, AddRemoveSubscriber) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  EXPECT_FALSE(is_being_observed());

  std::unique_ptr<Subscription> subscription = image->Subscribe();
  EXPECT_TRUE(is_being_observed());

  subscription.reset();
  EXPECT_FALSE(is_being_observed());
}

TEST_F(ThumbnailImageTest, AddRemoveMultipleObservers) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  EXPECT_FALSE(is_being_observed());

  std::unique_ptr<Subscription> subscription1 = image->Subscribe();
  EXPECT_TRUE(is_being_observed());

  std::unique_ptr<Subscription> subscription2 = image->Subscribe();
  EXPECT_TRUE(is_being_observed());

  subscription1.reset();
  EXPECT_TRUE(is_being_observed());

  subscription2.reset();
  EXPECT_FALSE(is_being_observed());
}

TEST_F(ThumbnailImageTest, AssignSkBitmapNotifiesObservers) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription1 = image->Subscribe();
  std::unique_ptr<Subscription> subscription2 = image->Subscribe();

  CallbackWaiter waiter1;
  subscription1->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(waiter1.callback()));

  CallbackWaiter waiter2;
  subscription2->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(waiter2.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);

  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());
}

TEST_F(ThumbnailImageTest, AssignSkBitmap_NotifiesObserversAgain) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription1 = image->Subscribe();
  std::unique_ptr<Subscription> subscription2 = image->Subscribe();

  CallbackWaiter waiter1;
  subscription1->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(waiter1.callback()));

  CallbackWaiter waiter2;
  subscription2->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(waiter2.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(bitmap, std::nullopt);

  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());

  waiter1.Reset();
  waiter2.Reset();

  image->AssignSkBitmap(std::move(bitmap), std::nullopt);

  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());
}

TEST_F(ThumbnailImageTest, AssignSkBitmap_NotifiesCompressedObservers) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription1 = image->Subscribe();
  std::unique_ptr<Subscription> subscription2 = image->Subscribe();

  CallbackWaiter waiter1;
  subscription1->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          waiter1.callback()));

  CallbackWaiter waiter2;
  subscription2->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          waiter2.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);

  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());
}

TEST_F(ThumbnailImageTest, AssignSkBitmap_NotifiesCompressedObserversAgain) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription1 = image->Subscribe();
  std::unique_ptr<Subscription> subscription2 = image->Subscribe();

  CallbackWaiter waiter1;
  subscription1->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          waiter1.callback()));

  CallbackWaiter waiter2;
  subscription2->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          waiter2.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(bitmap, std::nullopt);

  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());

  waiter1.Reset();
  waiter2.Reset();

  image->AssignSkBitmap(std::move(bitmap), std::nullopt);

  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());
}

TEST_F(ThumbnailImageTest, RequestThumbnailImage) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription1 = image->Subscribe();

  CallbackWaiter waiter1;
  subscription1->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(waiter1.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);
  waiter1.Wait();
  EXPECT_TRUE(waiter1.called());
  waiter1.Reset();

  std::unique_ptr<Subscription> subscription2 = image->Subscribe();

  CallbackWaiter waiter2;
  subscription2->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(waiter2.callback()));

  image->RequestThumbnailImage();
  waiter1.Wait();
  waiter2.Wait();
  EXPECT_TRUE(waiter1.called());
  EXPECT_TRUE(waiter2.called());
}

TEST_F(ThumbnailImageTest, RequestCompressedThumbnailData) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription = image->Subscribe();

  CallbackWaiter waiter;
  subscription->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          waiter.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);
  waiter.Wait();
  EXPECT_TRUE(waiter.called());
  waiter.Reset();

  image->RequestCompressedThumbnailData();
  waiter.Wait();
  EXPECT_TRUE(waiter.called());
}

TEST_F(ThumbnailImageTest, ClearThumbnailAfterAssignBitmap) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription = image->Subscribe();

  CallbackWaiter uncompressed_image_waiter;
  subscription->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(uncompressed_image_waiter.callback()));

  CallbackWaiter compressed_image_waiter;
  subscription->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          compressed_image_waiter.callback()));

  CallbackWaiter async_operation_finished_waiter;
  image->set_async_operation_finished_callback_for_testing(
      async_operation_finished_waiter.callback());

  // No observers should be notified if the thumbnail is cleared just
  // after assigning a bitmap.
  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);
  image->ClearData();
  async_operation_finished_waiter.Wait();
  EXPECT_TRUE(async_operation_finished_waiter.called());
  EXPECT_FALSE(uncompressed_image_waiter.called());
  EXPECT_FALSE(compressed_image_waiter.called());
}

TEST_F(ThumbnailImageTest, ClearExistingThumbnailNotifiesObservers) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription = image->Subscribe();

  CallbackWaiter uncompressed_image_waiter;
  subscription->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(uncompressed_image_waiter.callback()));

  CallbackWaiter compressed_image_waiter;
  subscription->SetCompressedImageCallback(
      base::IgnoreArgs<ThumbnailImage::CompressedThumbnailData>(
          compressed_image_waiter.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);
  compressed_image_waiter.Wait();
  uncompressed_image_waiter.Wait();
  EXPECT_TRUE(compressed_image_waiter.called());
  EXPECT_TRUE(uncompressed_image_waiter.called());
  compressed_image_waiter.Reset();
  uncompressed_image_waiter.Reset();

  image->ClearData();
  compressed_image_waiter.Wait();
  uncompressed_image_waiter.Wait();
  EXPECT_TRUE(compressed_image_waiter.called());
  EXPECT_TRUE(uncompressed_image_waiter.called());
}

// Makes sure a null dereference does not happen. Regression test for
// crbug.com/1159701.
TEST_F(ThumbnailImageTest, UnsubscribeAfterDelegateDestroyed) {
  auto delegate = std::make_unique<StubDelegate>();
  auto image = base::MakeRefCounted<ThumbnailImage>(delegate.get());

  std::unique_ptr<Subscription> subscription = image->Subscribe();

  // Normally |image| will notify its delegate when the last
  // subscription is destroyed. When there is no delegate it shouldn't
  // do anything.
  delegate.reset();
  subscription.reset();
}

// Ensures subscribers with a size hint get notified correctly on
// thumbnail clear. Regression test for crbug.com/1168483 where
// CropPreviewImage was called on blank thumbnails resulting in a
// DCHECK.
TEST_F(ThumbnailImageTest, DoesNotCropBlankThumbnails) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);

  std::unique_ptr<Subscription> subscription = image->Subscribe();
  subscription->SetSizeHint(
      gfx::Size(kTestBitmapWidth / 2, kTestBitmapHeight / 2));

  CallbackWaiter uncompressed_image_waiter;
  subscription->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(uncompressed_image_waiter.callback()));

  SkBitmap bitmap =
      gfx::test::CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(std::move(bitmap), std::nullopt);
  uncompressed_image_waiter.Wait();
  EXPECT_TRUE(uncompressed_image_waiter.called());
  uncompressed_image_waiter.Reset();

  image->ClearData();
  uncompressed_image_waiter.Wait();
  EXPECT_TRUE(uncompressed_image_waiter.called());
}
