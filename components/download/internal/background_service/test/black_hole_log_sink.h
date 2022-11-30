// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_BLACK_HOLE_LOG_SINK_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_BLACK_HOLE_LOG_SINK_H_

#include "components/download/internal/background_service/log_sink.h"

namespace download {
namespace test {

// A LogSink that does nothing with the calls to the interface.
class BlackHoleLogSink : public LogSink {
 public:
  BlackHoleLogSink() = default;

  BlackHoleLogSink(const BlackHoleLogSink&) = delete;
  BlackHoleLogSink& operator=(const BlackHoleLogSink&) = delete;

  ~BlackHoleLogSink() override = default;

  // LogSink implementation.
  void OnServiceStatusChanged() override;
  void OnServiceDownloadsAvailable() override;
  void OnServiceDownloadChanged(const std::string& guid) override;
  void OnServiceDownloadFailed(CompletionType completion_type,
                               const Entry& entry) override;
  void OnServiceRequestMade(DownloadClient client,
                            const std::string& guid,
                            DownloadParams::StartResult start_result) override;
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_BLACK_HOLE_LOG_SINK_H_
