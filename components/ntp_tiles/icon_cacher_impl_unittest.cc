// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher_impl.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service_impl.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/image/image_unittest_util.h"

using base::Bucket;
using image_fetcher::ImageFetcherParams;
using ::image_fetcher::MockImageFetcher;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnArg;

namespace ntp_tiles {
namespace {

const int kTestDipForServerRequests = 32;
const favicon_base::IconType kTestIconTypeForServerRequests =
    favicon_base::IconType::kTouchIcon;
const char kTestGoogleServerClientParam[] = "test_chrome";

ACTION(FailFetch) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*arg2), gfx::Image(),
                                image_fetcher::RequestMetadata()));
}

ACTION_P2(PassFetch, width, height) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*arg2), gfx::test::CreateImage(width, height),
                     image_fetcher::RequestMetadata()));
}

ACTION_P(Quit, run_loop) {
  run_loop->Quit();
}

// TODO(jkrcal): Split off large_icon_service.h and large_icon_service_impl.h.
// Use then mocks of FaviconService and LargeIconService instead of the real
// things.
class IconCacherTestBase : public ::testing::Test {
 protected:
  IconCacherTestBase()
      : favicon_service_(/*favicon_client=*/nullptr, &history_service_) {
    CHECK(history_dir_.CreateUniqueTempDir());
    CHECK(history_service_.Init(history::HistoryDatabaseParams(
        history_dir_.GetPath(), 0, 0, version_info::Channel::UNKNOWN)));
  }

  void PreloadIcon(const GURL& url,
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   int width,
                   int height) {
    favicon_service_.SetFavicons({url}, icon_url, icon_type,
                                 gfx::test::CreateImage(width, height));
  }

  bool IconIsCachedFor(const GURL& url, favicon_base::IconType icon_type) {
    return !GetCachedIconFor(url, icon_type).IsEmpty();
  }

  gfx::Image GetCachedIconFor(const GURL& url,
                              favicon_base::IconType icon_type) {
    base::CancelableTaskTracker tracker;
    gfx::Image image;
    base::RunLoop loop;
    favicon::GetFaviconImageForPageURL(
        &favicon_service_, url, icon_type,
        base::BindOnce(
            [](gfx::Image* image, base::RunLoop* loop,
               const favicon_base::FaviconImageResult& result) {
              *image = result.image;
              loop->Quit();
            },
            &image, &loop),
        &tracker);
    loop.Run();
    return image;
  }

  void WaitForHistoryThreadTasksToFinish() {
    base::RunLoop loop;
    base::MockOnceClosure done;
    EXPECT_CALL(done, Run()).WillOnce(Quit(&loop));
    history_service_.FlushForTest(done.Get());
    loop.Run();
  }

  void WaitForMainThreadTasksToFinish() {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  history::HistoryService history_service_;
  favicon::FaviconServiceImpl favicon_service_;
};

class IconCacherTestPopularSites : public IconCacherTestBase {
 protected:
  IconCacherTestPopularSites()
      : site_(std::u16string(),  // title, unused
              GURL("http://url.google/"),
              GURL("http://url.google/icon.png"),
              GURL("http://url.google/favicon.ico"),
              TileTitleSource::UNKNOWN),  // title_source, unused
        image_fetcher_(new ::testing::StrictMock<MockImageFetcher>),
        image_decoder_() {}

  void SetUp() override {
    if (ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::CleanupSharedInstance();
    }
    ON_CALL(mock_resource_delegate_, GetPathForResourcePack(_, _))
        .WillByDefault(ReturnArg<0>());
    ON_CALL(mock_resource_delegate_, GetPathForLocalePack(_, _))
        .WillByDefault(ReturnArg<0>());
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  void TearDown() override {
    if (ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::CleanupSharedInstance();
    }
    base::FilePath pak_path;
#if BUILDFLAG(IS_ANDROID)
    base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    base::PathService::Get(base::DIR_ASSETS, &pak_path);
#endif

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::kScaleFactorNone);
  }

  PopularSites::Site site_;
  std::unique_ptr<MockImageFetcher> image_fetcher_;
  image_fetcher::FakeImageDecoder image_decoder_;
  NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
};

TEST_F(IconCacherTestPopularSites, LargeCached) {
  base::HistogramTester histogram_tester;
  base::MockOnceClosure done;
  EXPECT_CALL(done, Run()).Times(0);

  PreloadIcon(site_.url, site_.large_icon_url,
              favicon_base::IconType::kTouchIcon, 128, 128);
  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  cacher.StartFetchPopularSites(site_, done.Get(), done.Get());
  WaitForMainThreadTasksToFinish();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestPopularSites, LargeNotCachedAndFetchSucceeded) {
  base::HistogramTester histogram_tester;
  base::MockOnceClosure done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                FetchImageAndData_(site_.large_icon_url, _, _, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Run()).WillOnce(Quit(&loop));
  }

  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  cacher.StartFetchPopularSites(site_, done.Get(), done.Get());
  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestPopularSites, SmallNotCachedAndFetchSucceeded) {
  site_.large_icon_url = GURL();

  base::MockOnceClosure done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_, FetchImageAndData_(site_.favicon_url, _, _, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Run()).WillOnce(Quit(&loop));
  }

  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  cacher.StartFetchPopularSites(site_, done.Get(), done.Get());
  loop.Run();
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::IconType::kFavicon));
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestPopularSites, LargeNotCachedAndFetchFailed) {
  base::HistogramTester histogram_tester;
  base::MockOnceClosure done;
  EXPECT_CALL(done, Run()).Times(0);
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                FetchImageAndData_(site_.large_icon_url, _, _, _))
        .WillOnce(FailFetch());
  }

  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  cacher.StartFetchPopularSites(site_, done.Get(), done.Get());
  WaitForMainThreadTasksToFinish();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kFavicon));
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestPopularSites, HandlesEmptyCallbacksNicely) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*image_fetcher_, FetchImageAndData_(_, _, _, _))
      .WillOnce(PassFetch(128, 128));
  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  cacher.StartFetchPopularSites(site_, base::NullCallback(),
                                base::NullCallback());
  WaitForHistoryThreadTasksToFinish();  // Writing the icon into the DB.
  WaitForMainThreadTasksToFinish();     // Finishing tasks after the DB write.
  // Even though the callbacks are not called, the icon gets written out.
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::IconType::kTouchIcon));
  // The histogram gets reported despite empty callbacks.
}

TEST_F(IconCacherTestPopularSites, ProvidesDefaultIconAndSucceedsWithFetching) {
  base::HistogramTester histogram_tester;
  // The returned data string is not used by the mocked decoder.
  ON_CALL(mock_resource_delegate_, GetRawDataResource(12345, _, _))
      .WillByDefault(Return(""));
  // It's not important when the image_fetcher's decoder is used to decode the
  // image but it must happen at some point.
  EXPECT_CALL(*image_fetcher_, GetImageDecoder())
      .WillOnce(Return(&image_decoder_));
  image_decoder_.SetDecodedImage(gfx::test::CreateImage(64, 64));
  base::MockOnceClosure preliminary_icon_available;
  base::MockOnceClosure icon_available;
  base::RunLoop default_loop;
  base::RunLoop fetch_loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                FetchImageAndData_(site_.large_icon_url, _, _, _))
        .WillOnce(PassFetch(128, 128));

    // Both callback are called async after the request but preliminary has to
    // preceed icon_available.
    EXPECT_CALL(preliminary_icon_available, Run())
        .WillOnce(Quit(&default_loop));
    EXPECT_CALL(icon_available, Run()).WillOnce(Quit(&fetch_loop));
  }

  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  site_.default_icon_resource = 12345;
  cacher.StartFetchPopularSites(site_, icon_available.Get(),
                                preliminary_icon_available.Get());

  default_loop.Run();  // Wait for the default image.
  EXPECT_THAT(
      GetCachedIconFor(site_.url, favicon_base::IconType::kTouchIcon).Size(),
      Eq(gfx::Size(64, 64)));  // Compares dimensions, not objects.

  // Let the fetcher continue and wait for the second call of the callback.
  fetch_loop.Run();  // Wait for the updated image.
  EXPECT_THAT(
      GetCachedIconFor(site_.url, favicon_base::IconType::kTouchIcon).Size(),
      Eq(gfx::Size(128, 128)));  // Compares dimensions, not objects.
}

TEST_F(IconCacherTestPopularSites, LargeNotCachedAndFetchPerformedOnlyOnce) {
  base::MockOnceClosure done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                FetchImageAndData_(site_.large_icon_url, _, _, _))
        .WillOnce(PassFetch(128, 128));
    // Success will be notified to both requests.
    EXPECT_CALL(done, Run()).WillOnce(Return()).WillOnce(Quit(&loop));
  }

  IconCacherImpl cacher(&favicon_service_, nullptr, std::move(image_fetcher_),
                        nullptr);
  cacher.StartFetchPopularSites(site_, done.Get(), done.Get());
  cacher.StartFetchPopularSites(site_, done.Get(), done.Get());
  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::IconType::kTouchIcon));
}

class IconCacherTestMostLikely : public IconCacherTestBase {
 protected:
  IconCacherTestMostLikely()
      : fetcher_for_large_icon_service_(
            std::make_unique<::testing::StrictMock<MockImageFetcher>>()),
        fetcher_for_icon_cacher_(
            std::make_unique<::testing::StrictMock<MockImageFetcher>>()) {}

  std::unique_ptr<MockImageFetcher> fetcher_for_large_icon_service_;
  std::unique_ptr<MockImageFetcher> fetcher_for_icon_cacher_;
};

TEST_F(IconCacherTestMostLikely, Cached) {
  GURL page_url("http://www.site.com");
  base::HistogramTester histogram_tester;

  GURL icon_url("http://www.site.com/favicon.png");
  PreloadIcon(page_url, icon_url, favicon_base::IconType::kTouchIcon, 128, 128);

  favicon::LargeIconServiceImpl large_icon_service(
      &favicon_service_, std::move(fetcher_for_large_icon_service_),
      kTestDipForServerRequests, kTestIconTypeForServerRequests,
      kTestGoogleServerClientParam);
  IconCacherImpl cacher(&favicon_service_, &large_icon_service,
                        std::move(fetcher_for_icon_cacher_), nullptr);

  base::MockOnceClosure done;
  EXPECT_CALL(done, Run()).Times(0);
  cacher.StartFetchMostLikely(page_url, done.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(IconIsCachedFor(page_url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(page_url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestMostLikely, NotCachedAndFetchSucceeded) {
  GURL page_url("http://www.site.com");
  base::HistogramTester histogram_tester;

  base::MockOnceClosure done;
  base::RunLoop loop;
  {
    InSequence s;

    EXPECT_CALL(*fetcher_for_large_icon_service_,
                FetchImageAndData_(_, _, _, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Run()).WillOnce(Quit(&loop));
  }

  favicon::LargeIconServiceImpl large_icon_service(
      &favicon_service_, std::move(fetcher_for_large_icon_service_),
      kTestDipForServerRequests, kTestIconTypeForServerRequests,
      kTestGoogleServerClientParam);
  IconCacherImpl cacher(&favicon_service_, &large_icon_service,
                        std::move(fetcher_for_icon_cacher_), nullptr);

  cacher.StartFetchMostLikely(page_url, done.Get());
  // Both these task runners need to be flushed in order to get |done| called by
  // running the main loop.
  WaitForHistoryThreadTasksToFinish();
  task_environment_.RunUntilIdle();

  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(page_url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(page_url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestMostLikely, NotCachedAndFetchFailed) {
  GURL page_url("http://www.site.com");
  base::HistogramTester histogram_tester;

  base::MockOnceClosure done;
  {
    InSequence s;

    EXPECT_CALL(*fetcher_for_large_icon_service_,
                FetchImageAndData_(_, _, _, _))
        .WillOnce(FailFetch());
    EXPECT_CALL(done, Run()).Times(0);
  }

  favicon::LargeIconServiceImpl large_icon_service(
      &favicon_service_, std::move(fetcher_for_large_icon_service_),
      kTestDipForServerRequests, kTestIconTypeForServerRequests,
      kTestGoogleServerClientParam);
  IconCacherImpl cacher(&favicon_service_, &large_icon_service,
                        std::move(fetcher_for_icon_cacher_), nullptr);

  cacher.StartFetchMostLikely(page_url, done.Get());
  // Both these task runners need to be flushed before flushing the main thread
  // queue in order to finish the work.
  WaitForHistoryThreadTasksToFinish();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(IconIsCachedFor(page_url, favicon_base::IconType::kFavicon));
  EXPECT_FALSE(IconIsCachedFor(page_url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestMostLikely, HandlesEmptyCallbacksNicely) {
  GURL page_url("http://www.site.com");

  EXPECT_CALL(*fetcher_for_large_icon_service_, FetchImageAndData_(_, _, _, _))
      .WillOnce(PassFetch(128, 128));

  favicon::LargeIconServiceImpl large_icon_service(
      &favicon_service_, std::move(fetcher_for_large_icon_service_),
      kTestDipForServerRequests, kTestIconTypeForServerRequests,
      kTestGoogleServerClientParam);
  IconCacherImpl cacher(&favicon_service_, &large_icon_service,
                        std::move(fetcher_for_icon_cacher_), nullptr);

  cacher.StartFetchMostLikely(page_url, base::NullCallback());

  // Finish the posted tasks on the main and the history thread to find out if
  // we can write the icon.
  task_environment_.RunUntilIdle();
  WaitForHistoryThreadTasksToFinish();
  // Continue with the work in large icon service - fetch and decode the data.
  task_environment_.RunUntilIdle();
  // Do the work on the history thread to write down the icon
  WaitForHistoryThreadTasksToFinish();
  // Finish the tasks on the main thread.
  task_environment_.RunUntilIdle();

  // Even though the callbacks are not called, the icon gets written out.
  EXPECT_FALSE(IconIsCachedFor(page_url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(page_url, favicon_base::IconType::kTouchIcon));
}

TEST_F(IconCacherTestMostLikely, NotCachedAndFetchPerformedOnlyOnce) {
  GURL page_url("http://www.site.com");

  base::MockOnceClosure done;
  base::RunLoop loop;
  {
    InSequence s;

    EXPECT_CALL(*fetcher_for_large_icon_service_,
                FetchImageAndData_(_, _, _, _))
        .WillOnce(PassFetch(128, 128));
    // Success will be notified to both requests.
    EXPECT_CALL(done, Run()).WillOnce(Return()).WillOnce(Quit(&loop));
  }

  favicon::LargeIconServiceImpl large_icon_service(
      &favicon_service_, std::move(fetcher_for_large_icon_service_),
      kTestDipForServerRequests, kTestIconTypeForServerRequests,
      kTestGoogleServerClientParam);
  IconCacherImpl cacher(&favicon_service_, &large_icon_service,
                        std::move(fetcher_for_icon_cacher_), nullptr);

  cacher.StartFetchMostLikely(page_url, done.Get());
  cacher.StartFetchMostLikely(page_url, done.Get());

  // Finish the posted tasks on the main and the history thread to find out if
  // we can write the icon.
  task_environment_.RunUntilIdle();
  WaitForHistoryThreadTasksToFinish();
  // Continue with the work in large icon service - fetch and decode the data.
  task_environment_.RunUntilIdle();
  // Do the work on the history thread to write down the icon
  WaitForHistoryThreadTasksToFinish();
  // Finish the tasks on the main thread.
  loop.Run();

  EXPECT_FALSE(IconIsCachedFor(page_url, favicon_base::IconType::kFavicon));
  EXPECT_TRUE(IconIsCachedFor(page_url, favicon_base::IconType::kTouchIcon));
}

}  // namespace
}  // namespace ntp_tiles
