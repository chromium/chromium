// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

class TestThumbnailImageDelegate : public ThumbnailImage::Delegate {
 public:
  TestThumbnailImageDelegate() = default;
  ~TestThumbnailImageDelegate() override = default;

  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {
    is_being_observed_ = is_being_observed;
  }

  bool is_being_observed() const { return is_being_observed_; }

 private:
  bool is_being_observed_ = false;
};

class ThumbnailTrackerTest : public ::testing::Test,
                             public ThumbnailImage::Delegate {
 protected:
  ThumbnailTrackerTest()
      : thumbnail_tracker_(
            thumbnail_updated_callback_.Get(),
            base::BindRepeating(&ThumbnailTrackerTest::GetTestingThumbnail,
                                base::Unretained(this))) {}

  static SkBitmap CreateTestingBitmap() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1, true);
    bitmap.eraseColor(SK_ColorBLACK);
    bitmap.setImmutable();
    return bitmap;
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    auto contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    CreateSessionServiceTabHelper(contents.get());
    return contents;
  }

  scoped_refptr<ThumbnailImage> GetTestingThumbnail(
      content::WebContents* contents) {
    return tab_thumbnails_[contents].thumbnail_image;
  }

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {}

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;

  base::MockCallback<ThumbnailTracker::ThumbnailUpdatedCallback>
      thumbnail_updated_callback_;

  struct ThumbnailData {
    ThumbnailData()
        : thumbnail_image(base::MakeRefCounted<ThumbnailImage>(&delegate)) {}

    TestThumbnailImageDelegate delegate;
    scoped_refptr<ThumbnailImage> thumbnail_image;
  };
  std::map<content::WebContents*, ThumbnailData> tab_thumbnails_;

  ThumbnailTracker thumbnail_tracker_;
};

}  // namespace

using ::testing::_;

TEST_F(ThumbnailTrackerTest, AddTabGetsCurrentThumbnail) {
  auto contents = CreateWebContents();
  auto thumbnail = GetTestingThumbnail(contents.get());

  // Set the thumbnail image and wait for it to be stored.
  base::RunLoop encode_loop;
  thumbnail->set_async_operation_finished_callback_for_testing(
      encode_loop.QuitClosure());
  thumbnail->AssignSkBitmap(CreateTestingBitmap());
  encode_loop.Run();

  // Verify that AddTab() gets the current image. This should happen
  // immediately.
  EXPECT_CALL(thumbnail_updated_callback_, Run(contents.get(), _)).Times(1);
  thumbnail->set_async_operation_finished_callback_for_testing(
      base::RepeatingClosure());
  thumbnail_tracker_.AddTab(contents.get());
  EXPECT_TRUE(tab_thumbnails_[contents.get()].delegate.is_being_observed());
}

TEST_F(ThumbnailTrackerTest, PropagatesThumbnailUpdate) {
  auto contents1 = CreateWebContents();
  auto thumbnail1 = GetTestingThumbnail(contents1.get());
  auto contents2 = CreateWebContents();
  auto thumbnail2 = GetTestingThumbnail(contents2.get());

  // Since no thumbnail image exists yet, this shouldn't notify our callback.
  thumbnail_tracker_.AddTab(contents1.get());
  thumbnail_tracker_.AddTab(contents2.get());

  {
    ::testing::InSequence seq;
    EXPECT_CALL(thumbnail_updated_callback_, Run(contents1.get(), _)).Times(1);
    EXPECT_CALL(thumbnail_updated_callback_, Run(contents2.get(), _)).Times(1);
  }

  base::RunLoop first_update_loop;
  thumbnail1->set_async_operation_finished_callback_for_testing(
      first_update_loop.QuitClosure());
  thumbnail1->AssignSkBitmap(CreateTestingBitmap());
  first_update_loop.Run();

  base::RunLoop second_update_loop;
  thumbnail2->set_async_operation_finished_callback_for_testing(
      second_update_loop.QuitClosure());
  thumbnail2->AssignSkBitmap(CreateTestingBitmap());
  second_update_loop.Run();
}

TEST_F(ThumbnailTrackerTest, StopsObservingOnTabClose) {
  auto contents = CreateWebContents();
  auto thumbnail = GetTestingThumbnail(contents.get());
  thumbnail_tracker_.AddTab(contents.get());

  // |thumbnail| is still valid, but |thumbnail_tracker_| should stop watching
  // it when |contents| goes away.
  EXPECT_CALL(thumbnail_updated_callback_, Run(_, _)).Times(0);
  contents.reset();

  base::RunLoop update_loop;
  thumbnail->set_async_operation_finished_callback_for_testing(
      update_loop.QuitClosure());
  thumbnail->AssignSkBitmap(CreateTestingBitmap());
  update_loop.Run();
}

TEST_F(ThumbnailTrackerTest, RemoveTabStopsObservingThumbnail) {
  auto contents = CreateWebContents();
  auto thumbnail = GetTestingThumbnail(contents.get());
  thumbnail_tracker_.AddTab(contents.get());
  thumbnail_tracker_.RemoveTab(contents.get());
  EXPECT_FALSE(tab_thumbnails_[contents.get()].delegate.is_being_observed());
}
