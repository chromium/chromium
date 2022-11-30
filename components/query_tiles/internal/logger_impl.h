// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_LOGGER_IMPL_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_LOGGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/query_tiles/logger.h"

#include "base/observer_list.h"
#include "components/query_tiles/internal/log_sink.h"
#include "components/query_tiles/internal/log_source.h"

namespace query_tiles {

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
  base::Value GetTileData() override;

  // LogSink implementation.
  void OnServiceStatusChanged() override;
  void OnTileDataAvailable() override;

  raw_ptr<LogSource> log_source_{nullptr};
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_LOGGER_IMPL_H_
