// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_SERVICE_ADAPTOR_H_
#define CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_SERVICE_ADAPTOR_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos::cfm {

// Abstract class that provides convenience methods, allowing new CfM Services
// to register with the |CfmServiceContext| through its mojo |Interface|
class COMPONENT_EXPORT(CHROMEOS_CFMSERVICE) ServiceAdaptor
    : public mojom::CfmServiceAdaptor {
 public:
  using GetServiceCallback =
      mojom::CfmServiceContext::RequestBindServiceCallback;

  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Called when the Service Adaptor has successfully connected to the
    // |mojom::CfmServiceContext|
    virtual void OnAdaptorConnect(bool success);

    // Called if the mojom connection to the primary |mojom::CfmServiceContext|
    // is disrupted.
    virtual void OnAdaptorDisconnect();

    // Called when attempting to Bind a mojom using using a message pipe of the
    // given types PendingReceiver.
    virtual void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) = 0;

   protected:
    Delegate() = default;
  };

  ServiceAdaptor(std::string interface_name, Delegate* delegate);
  ServiceAdaptor(const ServiceAdaptor&) = delete;
  ServiceAdaptor& operator=(const ServiceAdaptor&) = delete;
  ~ServiceAdaptor() override;

  // Returns the primary |mojom::CfmServiceContext|
  virtual mojom::CfmServiceContext* GetContext();

  // Binds a |mojo::Remote| to the primary |mojom::CfmServiceContext|
  virtual void BindServiceAdaptor();

  template <typename Interface>
  void GetService(mojo::Remote<Interface>& remote,
                  GetServiceCallback callback) {
    GetService(Interface::Name_,
               std::move(remote.BindNewPipeAndPassReceiver()).PassPipe(),
               std::move(callback));
  }

  virtual void GetService(std::string interface_name,
                          mojo::ScopedMessagePipeHandle receiver_pipe,
                          GetServiceCallback callback);

 protected:
  // Forward |mojom::CfmServiceAdaptor| implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

  // Called when the Service Adaptor has successfully connected to the
  // |mojom::CfmServiceContext|
  virtual void OnAdaptorConnect(bool success);

  // Called if the mojom connection to the primary |mojom::CfmServiceContext| is
  // disrupted.
  virtual void OnAdaptorDisconnect();

 private:
  // Name of the mojom interface (i.e Interface::Name_) this service adaptor is
  // proxying bind request for.
  const std::string interface_name_;

  const raw_ptr<Delegate> delegate_;

  mojo::Remote<mojom::CfmServiceContext> context_;
  mojo::Receiver<mojom::CfmServiceAdaptor> adaptor_{this};

  base::WeakPtrFactory<ServiceAdaptor> weak_ptr_factory_{this};
};

}  // namespace chromeos::cfm

#endif  // CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_SERVICE_ADAPTOR_H_
