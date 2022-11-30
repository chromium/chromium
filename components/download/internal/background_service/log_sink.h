// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOG_SINK_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOG_SINK_H_

#include "components/download/internal/background_service/constants.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/public/background_service/download_params.h"

namespace download {

struct Entry;

// A destination for all interesting events from internal components.
class LogSink {
 public:
  virtual ~LogSink() = default;

  // To be called whenever the StartupStatus/Controller::State changes.
  virtual void OnServiceStatusChanged() = 0;

  // To be called whenever the list of underlying Entry objects is ready to be
  // queried by external log entities.
  virtual void OnServiceDownloadsAvailable() = 0;

  // To be called whenever the state the download represented by |guid| changes.
  virtual void OnServiceDownloadChanged(const std::string& guid) = 0;

  // To be called whenever a download fails.
  virtual void OnServiceDownloadFailed(CompletionType completion_type,
                                       const Entry& entry) = 0;

  // To be called whenever a request has been made of the download service.
  virtual void OnServiceRequestMade(
      DownloadClient client,
      const std::string& guid,
      DownloadParams::StartResult start_result) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOG_SINK_H_
