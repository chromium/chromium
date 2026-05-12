// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_job_factory.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_job.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/common/url_loader_factory_provider.h"
#include "net/http/http_connection_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace download {
namespace {

class DownloadJobFactoryTest : public testing::Test {
 public:
  DownloadJobFactoryTest() {
    feature_list_.InitAndEnableFeature(features::kParallelDownloading);
  }

  // Builds a DownloadCreateInfo that satisfies every condition checked by
  // IsParallelizableDownload(), so the factory will choose a
  // ParallelDownloadJob unless an additional disqualifier (e.g. Service
  // Worker interception) flips the decision.
  std::unique_ptr<DownloadCreateInfo> CreateParallelizableInfo() {
    auto info = std::make_unique<DownloadCreateInfo>();
    info->url_chain.emplace_back("https://example.com/file.bin");
    info->etag = "\"abc\"";
    info->total_bytes = 100 * 1024 * 1024;
    info->accept_range = RangeRequestSupportType::kSupport;
    info->connection_info = net::HttpConnectionInfo::kHTTP1_1;
    return info;
  }

  std::unique_ptr<DownloadJob> CreateJob(const DownloadCreateInfo& info) {
    return DownloadJobFactory::CreateJob(
        &download_item_, base::DoNothing(), info,
        /*is_save_package_download=*/false,
        URLLoaderFactoryProvider::GetNullPtr(),
        /*wake_lock_provider_binder=*/base::NullCallback());
  }

  void SetUp() override {
    ON_CALL(download_item_, GetDownloadFile()).WillByDefault(Return(nullptr));
    ON_CALL(download_item_, GetReceivedBytes()).WillByDefault(Return(0));
    ON_CALL(download_item_, GetReceivedSlices())
        .WillByDefault(ReturnRef(empty_slices_));
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  NiceMock<MockDownloadItem> download_item_;
  DownloadItem::ReceivedSlices empty_slices_;
};

// Anchors the negative test below: confirms the fixture's create_info actually
// produces a parallelizable job when nothing disqualifies it.
TEST_F(DownloadJobFactoryTest, CreatesParallelJobWhenAllConditionsMet) {
  auto info = CreateParallelizableInfo();
  auto job = CreateJob(*info);
  EXPECT_TRUE(job->IsParallelizable());
}

// A Service Worker fetch handler runs once and its single-use loader cannot
// be re-driven for parallel range slices, so the factory must fall back to a
// non-parallel job whenever the response was SW-intercepted.
TEST_F(DownloadJobFactoryTest, DisablesParallelJobWhenFetchedViaServiceWorker) {
  auto info = CreateParallelizableInfo();
  info->fetched_via_service_worker = true;
  auto job = CreateJob(*info);
  EXPECT_FALSE(job->IsParallelizable());
}

}  // namespace
}  // namespace download
