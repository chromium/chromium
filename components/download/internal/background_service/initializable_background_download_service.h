// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_INITIALIZABLE_BACKGROUND_DOWNLOAD_SERVICE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_INITIALIZABLE_BACKGROUND_DOWNLOAD_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "components/download/public/background_service/background_download_service.h"

namespace download {

// A Background download service that needs to be explicitly initialized.
class InitializableBackgroundDownloadService
    : public BackgroundDownloadService {
 public:
  // Initializes the background download service.
  virtual void Initialize(base::OnceClosure callback) = 0;

  ~InitializableBackgroundDownloadService() override = default;

 protected:
  InitializableBackgroundDownloadService() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_INITIALIZABLE_BACKGROUND_DOWNLOAD_SERVICE_H_
