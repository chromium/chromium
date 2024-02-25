// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOG_SOURCE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOG_SOURCE_H_

#include <optional>
#include <utility>
#include <vector>

#include "components/download/internal/background_service/controller.h"

namespace download {

struct DriverEntry;
struct Entry;
struct StartupStatus;

// A source for all relevant logging data.  LoggerImpl will pull from an
// instance of LogSource to push relevant log information to observers.
class LogSource {
 public:
  using EntryDetails = std::pair<const Entry*, std::optional<DriverEntry>>;
  using EntryDetailsList = std::vector<EntryDetails>;

  virtual ~LogSource() = default;

  // Returns the state of the Controller (see Controller::State).
  virtual Controller::State GetControllerState() = 0;

  // Returns the current StartupStatus of the service.
  virtual const StartupStatus& GetStartupStatus() = 0;

  // Returns the current list of (Driver)Entry objects the service is tracking.
  virtual EntryDetailsList GetServiceDownloads() = 0;

  // Returns the (Driver)Entry object representing the donwnload at |guid|.
  virtual std::optional<EntryDetails> GetServiceDownload(
      const std::string& guid) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_LOG_SOURCE_H_
