// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/mock_download_progress_observer.h"

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"

namespace optimization_guide {

MockDownloadProgressObserver::MockDownloadProgressObserver() = default;
MockDownloadProgressObserver::~MockDownloadProgressObserver() = default;

mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
MockDownloadProgressObserver::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockDownloadProgressObserver::ExpectReceivedUpdate(
    uint64_t expected_downloaded_bytes,
    uint64_t expected_total_bytes,
    base::OnceClosure callback) {
  EXPECT_CALL(*this, OnDownloadProgressUpdate(testing::_, testing::_))
      .WillOnce([callback = std::move(callback), expected_downloaded_bytes,
                 expected_total_bytes](uint64_t downloaded_bytes,
                                       uint64_t total_bytes) mutable {
        EXPECT_EQ(downloaded_bytes, expected_downloaded_bytes);
        EXPECT_EQ(total_bytes, expected_total_bytes);
        std::move(callback).Run();
      });
}

void MockDownloadProgressObserver::ExpectReceivedUpdate(
    uint64_t expected_downloaded_bytes,
    uint64_t expected_total_bytes) {
  base::RunLoop download_progress_run_loop;
  ExpectReceivedUpdate(expected_downloaded_bytes, expected_total_bytes,
                       download_progress_run_loop.QuitClosure());
  download_progress_run_loop.Run();
}

void MockDownloadProgressObserver::ExpectReceivedNormalizedUpdate(
    uint64_t expected_downloaded_bytes,
    uint64_t expected_total_bytes,
    base::OnceClosure callback) {
  ExpectReceivedUpdate(NormalizeModelDownloadProgress(expected_downloaded_bytes,
                                                      expected_total_bytes),
                       kNormalizedDownloadProgressMax, std::move(callback));
}

void MockDownloadProgressObserver::ExpectReceivedNormalizedUpdate(
    uint64_t expected_downloaded_bytes,
    uint64_t expected_total_bytes) {
  ExpectReceivedUpdate(NormalizeModelDownloadProgress(expected_downloaded_bytes,
                                                      expected_total_bytes),
                       kNormalizedDownloadProgressMax);
}

void MockDownloadProgressObserver::ExpectNoUpdate() {
  EXPECT_CALL(*this, OnDownloadProgressUpdate(testing::_, testing::_)).Times(0);
}

}  // namespace optimization_guide
