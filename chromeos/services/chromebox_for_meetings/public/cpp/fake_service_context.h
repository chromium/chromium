// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONTEXT_H_
#define CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONTEXT_H_

#include "base/functional/bind.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace cfm {

class FakeCfmServiceContext
    : public mojom::CfmServiceContext {
 public:
  using FakeProvideAdaptorCallback = base::OnceCallback<void(
      const std::string& interface_name,
      mojo::PendingRemote<mojom::CfmServiceAdaptor>
          adaptor_remote,
      ProvideAdaptorCallback callback)>;

  using FakeRequestBindServiceCallback =
      base::OnceCallback<void(const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle receiver_pipe,
                              RequestBindServiceCallback callback)>;

  FakeCfmServiceContext();
  FakeCfmServiceContext(const FakeCfmServiceContext&) = delete;
  FakeCfmServiceContext& operator=(const FakeCfmServiceContext&) = delete;
  ~FakeCfmServiceContext() override;

  void ProvideAdaptor(
      const std::string& interface_name,
      mojo::PendingRemote<mojom::CfmServiceAdaptor>
          adaptor_remote,
      ProvideAdaptorCallback callback) override;

  void RequestBindService(const std::string& interface_name,
                          mojo::ScopedMessagePipeHandle receiver_pipe,
                          RequestBindServiceCallback callback) override;

  void SetFakeProvideAdaptorCallback(FakeProvideAdaptorCallback callback);

  void SetFakeRequestBindServiceCallback(
      FakeRequestBindServiceCallback callback);

 private:
  FakeProvideAdaptorCallback provide_adaptor_callback_;
  FakeRequestBindServiceCallback request_bind_service_callback_;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONTEXT_H_
