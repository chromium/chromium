// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_LOGGER_H_
#define COMPONENTS_QUERY_TILES_LOGGER_H_

namespace base {
class Value;
}

namespace query_tiles {

// A helper class to expose internals of the query-tiles service to a logging
// component and/or debug UI.
class Logger {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called whenever the service status changes.
    virtual void OnServiceStatusChanged(const base::Value& status) = 0;

    // Called when the tile group data is available.
    virtual void OnTileDataAvailable(const base::Value& status) = 0;
  };

  virtual ~Logger() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  Logger(const Logger& other) = delete;
  Logger& operator=(const Logger& other) = delete;

  // Returns the current status of query tile service.
  // The serialized format will be:
  // {
  //  fetcherStatus:[INITIAL, SUCCESS, FAIL, SUSPEND]
  //  groupStatus:[SUCCESS, UN_INIT, NO_TILES, DB_FAIL]
  // }
  virtual base::Value GetServiceStatus() = 0;

  // Returns the formatted raw data stored in database.
  // The serialized format will be:
  // {
  //  groupInfo(string)
  //  tileProto(string)
  // }
  virtual base::Value GetTileData() = 0;

 protected:
  Logger() = default;
};
}  // namespace query_tiles
#endif  // COMPONENTS_QUERY_TILES_LOGGER_H_
