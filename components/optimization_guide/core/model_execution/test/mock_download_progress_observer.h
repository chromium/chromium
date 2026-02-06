// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_DOWNLOAD_PROGRESS_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_DOWNLOAD_PROGRESS_OBSERVER_H_

#include <cstdint>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class MockDownloadProgressObserver
    : public on_device_model::mojom::DownloadObserver {
 public:
  MockDownloadProgressObserver();
  ~MockDownloadProgressObserver() override;
  MockDownloadProgressObserver(const MockDownloadProgressObserver&) = delete;
  MockDownloadProgressObserver& operator=(const MockDownloadProgressObserver&) =
      delete;

  mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
  BindNewPipeAndPassRemote();

  // `on_device_model::mojom::DownloadObserver` implementation.
  MOCK_METHOD(void,
              OnDownloadProgressUpdate,
              (uint64_t downloaded_bytes, uint64_t total_bytes),
              (override));

  // Expects that the next `OnDownloadProgressUpdate` is called with
  // `expected_downloaded_bytes` and `expected_total_bytes`. Once it receives
  // an update, calls `callback`.
  void ExpectReceivedUpdate(uint64_t expected_downloaded_bytes,
                            uint64_t expected_total_bytes,
                            base::OnceClosure callback);

  // Overload that waits until the update is received.
  void ExpectReceivedUpdate(uint64_t expected_downloaded_bytes,
                            uint64_t expected_total_bytes);

  // Same as `ExpectReceivedUpdate` except it normalizes
  // `expected_downloaded_bytes` and `expected_total_bytes`.
  void ExpectReceivedNormalizedUpdate(uint64_t expected_downloaded_bytes,
                                      uint64_t expected_total_bytes,
                                      base::OnceClosure callback);

  // Overload that waits until the update is received.
  void ExpectReceivedNormalizedUpdate(uint64_t expected_downloaded_bytes,
                                      uint64_t expected_total_bytes);

  void ExpectNoUpdate();

 private:
  mojo::Receiver<on_device_model::mojom::DownloadObserver> receiver_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_DOWNLOAD_PROGRESS_OBSERVER_H_
