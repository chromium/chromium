// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOGGER_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOGGER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/download/internal/background_service/log_sink.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/logger.h"

namespace base {
class Value;
}

namespace download {

class LogSource;
struct Entry;

// The internal Logger implementation.  Note that this Logger will not do any
// actual work in response to LogSink requests if there are no Observers
// registered.  Any calls to the Logger API will still be honored though.
class LoggerImpl : public Logger, public LogSink {
 public:
  LoggerImpl();
  ~LoggerImpl() override;

  void SetLogSource(LogSource* log_source);

 private:
  // Logger implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::Value GetServiceStatus() override;
  base::Value GetServiceDownloads() override;

  // LogSink implementation.
  void OnServiceStatusChanged() override;
  void OnServiceDownloadsAvailable() override;
  void OnServiceDownloadChanged(const std::string& guid) override;
  void OnServiceDownloadFailed(CompletionType completion_type,
                               const Entry& entry) override;
  void OnServiceRequestMade(DownloadClient client,
                            const std::string& guid,
                            DownloadParams::StartResult start_result) override;

  LogSource* log_source_;
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(LoggerImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOGGER_IMPL_H_
