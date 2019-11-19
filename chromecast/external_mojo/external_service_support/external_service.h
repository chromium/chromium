// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_SERVICE_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chromecast {
namespace external_service_support {

// API for external (non-Chromium process) Mojo services.
class ExternalService : public external_mojo::mojom::ExternalService {
 public:
  ExternalService();
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
      base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>)>
          bind_callback) {
    RemoveInterface<Interface>();
    AddInterface(Interface::Name_, std::make_unique<CallbackBinder<Interface>>(
                                       std::move(bind_callback)));
  }

  // Removes an interface, preventing new bindings from being created. Does not
  // affect existing bindings.
  template <typename Interface>
  void RemoveInterface() {
    RemoveInterface(Interface::Name_);
  }

 private:
  class Binder {
   public:
    virtual ~Binder() = default;

    // Provides an abstract interface to the templated callback-based binder
    // below.
    virtual void BindInterface(
        const std::string& interface_name,
        mojo::ScopedMessagePipeHandle interface_pipe) = 0;
  };

  template <typename Interface>
  class CallbackBinder : public Binder {
   public:
    CallbackBinder(
        base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>)>
            bind_callback)
        : bind_callback_(bind_callback) {}

   private:
    // Binder implementation:
    void BindInterface(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
      mojo::InterfaceRequest<Interface> request(std::move(interface_pipe));
      bind_callback_.Run(std::move(request));
    }

    base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>)>
        bind_callback_;

    DISALLOW_COPY_AND_ASSIGN(CallbackBinder);
  };

  void AddInterface(const std::string& interface_name,
                    std::unique_ptr<Binder> binder);
  void RemoveInterface(const std::string& interface_name);

  // external_mojo::mojom::ExternalService implementation:
  void OnBindInterface(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  std::map<std::string, std::unique_ptr<Binder>> binders_;
  mojo::Receiver<external_mojo::mojom::ExternalService> service_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ExternalService);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_SERVICE_H_
