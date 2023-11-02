// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TEST_EMPTY_LOGGER_H_
#define COMPONENTS_QUERY_TILES_TEST_EMPTY_LOGGER_H_

#include "components/query_tiles/logger.h"
namespace query_tiles {
namespace test {

// A Logger that does nothing.
class EmptyLogger : public Logger {
 public:
  EmptyLogger() = default;
  ~EmptyLogger() override = default;

  // Logger implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::Value GetServiceStatus() override;
  base::Value GetTileData() override;
};

}  // namespace test
}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_TEST_EMPTY_LOGGER_H_
