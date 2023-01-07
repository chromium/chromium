// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_H_

#include <string>

#include "components/download/internal/background_service/initializable_background_download_service.h"
#include "components/download/public/background_service/clients.h"

namespace download {

// The core Controller responsible for gluing various BackgroundDownloadService
// components together to manage the active downloads.
class Controller : public InitializableBackgroundDownloadService {
 public:
  enum class State {
    // The Controller has been created but has not been initialized yet.  It
    // cannot be used.
    CREATED = 1,

    // The Controller has been created and Initialize() has been called but has
    // not yet finished.  It cannot be used.
    INITIALIZING = 2,

    // The Controller has been created and initialized.  It can be used.
    READY = 3,

    // The Controller failed to initialize and is in the process of recovering.
    // It cannot be used.
    RECOVERING = 4,

    // The Controller was unable to recover and is unusable this session.
    UNAVAILABLE = 5,
  };

  Controller() = default;

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  ~Controller() override = default;

  // Returns the status of Controller.
  virtual State GetState() = 0;

  // Exposes the owner of the download request for |guid| if one exists.
  // Otherwise returns DownloadClient::INVALID for an unowned entry.
  virtual DownloadClient GetOwnerOfDownload(const std::string& guid) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_H_
