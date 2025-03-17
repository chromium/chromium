// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_LOGGER_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_LOGGER_IMPL_H_

#include "components/data_sharing/public/logger.h"

namespace data_sharing {

// Implementation of the Data Sharing Logger.
class LoggerImpl : public Logger {
 public:
  LoggerImpl();
  ~LoggerImpl() override;

  LoggerImpl(const LoggerImpl&) = delete;
  LoggerImpl& operator=(const LoggerImpl&) = delete;

  // Logger implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool ShouldEnableDebugLogs() override;
  void Log(base::Time event_time,
           logger_common::mojom::LogSource log_source,
           const std::string& source_file,
           int source_line,
           const std::string& message) override;

 private:
  // Whether or not to always log regardless of whether or not any observers are
  // registered.
  const bool always_log_;

  // Running list of log entries (gets dropped as soon as the system no longer
  // needs to be logging).
  std::vector<Entry> logs_;
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_LOGGER_IMPL_H_
