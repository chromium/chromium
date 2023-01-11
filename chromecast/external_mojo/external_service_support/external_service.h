// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_SERVICE_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "chromecast/mojo/interface_bundle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chromecast {
namespace external_service_support {

// API for external (non-Chromium process) Mojo services.
class ExternalService : public external_mojo::mojom::ExternalService {
 public:
  ExternalService();

  ExternalService(const ExternalService&) = delete;
  ExternalService& operator=(const ExternalService&) = delete;

  ~ExternalService() override;

  // Returns the Mojo receiver for this service.
  mojo::PendingRemote<external_mojo::mojom::ExternalService> GetReceiver();

  // Adds an interface that users of this service may bind to. To avoid races
  // where the service is registered but interfaces cannot be bound by other
  // processes/services, add all interfaces before registering this service.
  // The |bind_callback| will be called once for each bind attempt. The callback
  // will not be called after this ExternalService instance is destroyed.
  template <typename Interface>
  void AddInterface(
      base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>
          bind_callback) {
    RemoveInterface<Interface>();
    bundle_.AddBinder(std::move(bind_callback));
  }

  // Convenience method for exposing an interface. The implementation must
  // outlive the service or be explicitly removed before the implementation is
  // destroyed.
  template <typename Interface>
  void AddInterface(Interface* interface) {
    RemoveInterface<Interface>();
    bundle_.AddInterface<Interface>(interface);
  }

  // Removes an interface, preventing new bindings from being created. Does not
  // affect existing bindings.
  template <typename Interface>
  void RemoveInterface() {
    bundle_.RemoveInterface<Interface>();
  }

  InterfaceBundle* bundle() { return &bundle_; }

 private:
  // external_mojo::mojom::ExternalService implementation:
  void OnBindInterface(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  InterfaceBundle bundle_;
  mojo::Receiver<external_mojo::mojom::ExternalService> service_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_SERVICE_H_
