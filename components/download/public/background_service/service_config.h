// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_SERVICE_CONFIG_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_SERVICE_CONFIG_H_

#include <stdint.h>

namespace base {
class TimeDelta;
}  // namespace base

namespace download {

// Contains the configuration used by this DownloadService for internal download
// operations.  Meant to be used by Clients for any tweaking they might want to
// do based on the configuration parameters.
class ServiceConfig {
 public:
  virtual ~ServiceConfig() = default;

  // The maximum number of downloads that can be outstanding per Client.  Any
  // Client attempting to schedule more downloads than this limit will receive a
  // DownloadParams::StartResult::BACKOFF return value.
  virtual uint32_t GetMaxScheduledDownloadsPerClient() const = 0;

  // The maximum number of downloads the DownloadService can have currently in
  // Active or Paused states, shared by all clients.
  virtual uint32_t GetMaxConcurrentDownloads() const = 0;

  // Returns the minimum amount of time the DownloadService will wait before
  // automatically erasing any files that remain on disk in the same place with
  // the same name for a completed download.  It is up to the Client to move or
  // consume the file before this time limit is reached.
  virtual const base::TimeDelta& GetFileKeepAliveTime() const = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_SERVICE_CONFIG_H_
