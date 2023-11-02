// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_LOGGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_LOGGER_H_

#include "base/values.h"

namespace download {

// A helper class to expose internals of the downloads system to a logging
// component and/or debug UI.
class Logger {
 public:
  // An Observer to be notified of any DownloadService changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called whenever the status of the Download Service changes.  This will
    // have the same data as |GetServiceStatus()|.
    virtual void OnServiceStatusChanged(
        const base::Value::Dict& service_status) = 0;

    // Called when the Download Service is able to notify observers of the list
    // of currently tracked downloads.  This will have the same data as
    // |GetServiceDownloads()|.
    virtual void OnServiceDownloadsAvailable(
        const base::Value::List& service_downloads) = 0;

    // Called when the state of a download has changed.  Format of
    // |service_download| is the same as |GetServiceDownloads()|, except not a
    // list.
    virtual void OnServiceDownloadChanged(
        const base::Value::Dict& service_download) = 0;

    // Called when a download has failed.  Format of |service_download| is the
    // same as |GetServiceDownloads()|, except not a list.
    virtual void OnServiceDownloadFailed(
        const base::Value::Dict& service_download) = 0;

    // Called when a request is made of the download service.  Format of
    // |service_request| is:
    // {
    //   client: string,
    //   guid: string,
    //   result: [ACCEPTED,BACKOFF,UNEXPECTED_CLIENT,UNEXPECTED_GUID,
    //            CLIENT_CANCELLED,INTERNAL_ERROR]
    // }
    virtual void OnServiceRequestMade(
        const base::Value::Dict& service_request) = 0;
  };

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  virtual ~Logger() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the current status of the Download Service.  The serialized format
  // will be:
  // {
  //   serviceState: string [CREATED,INITIALIZING,READY,RECOVERING,UNAVAILABLE],
  //   modelStatus: string [OK,BAD,UNKNOWN],
  //   driverStatus: string [OK,BAD,UNKNOWN],
  //   fileMonitorStatus: string [OK,BAD,UNKNOWN]
  // }
  virtual base::Value::Dict GetServiceStatus() = 0;

  // Returns the current list of downloads the Download Service is aware of.
  // The serialized format will be a list of:
  // {
  //   client: string,
  //   guid: string,
  //   state: string [NEW,AVAILABLE,ACTIVE,PAUSED,COMPLETE],
  //   url: string,
  //   file_path: optional string,
  //   bytes_downloaded: number,
  //   result: optional [SUCCEED,FAIL,ABORT,TIMEOUT,UNKNOWNL,CANCEL,
  //                     OUT_OF_RETRIES,OUT_OF_RESUMPTIONS],
  //   driver: {
  //     state: string [IN_PROGRESS,COMPLETE,CANCELLED,INTERRUPTED],
  //     paused: boolean,
  //     done: boolean,
  //   }
  // }
  virtual base::Value::List GetServiceDownloads() = 0;

 protected:
  Logger() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_LOGGER_H_
