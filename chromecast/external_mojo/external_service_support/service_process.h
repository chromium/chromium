// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_SERVICE_PROCESS_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_SERVICE_PROCESS_H_

#include <memory>

namespace chromecast {
namespace external_service_support {

class ExternalConnector;

// Provides an entrypoint for external processes that are using Mojo with
// standalone_service_main.cc.
class ServiceProcess {
 public:
  virtual ~ServiceProcess() = default;

  // Entrypoint from standalone_service_main.cc.
  static std::unique_ptr<ServiceProcess> Create(ExternalConnector* connector);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_SERVICE_PROCESS_H_
