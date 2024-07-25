// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"

namespace chromeos {
namespace cfm {

FakeCfmServiceContext::FakeCfmServiceContext() = default;
FakeCfmServiceContext::~FakeCfmServiceContext() = default;

void FakeCfmServiceContext::ProvideAdaptor(
    const std::string& interface_name,
    mojo::PendingRemote<mojom::CfmServiceAdaptor> adaptor_remote,
    ProvideAdaptorCallback callback) {
  std::move(provide_adaptor_callback_)
      .Run(std::move(interface_name), std::move(adaptor_remote),
           std::move(callback));
}

void FakeCfmServiceContext::RequestBindService(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe,
    RequestBindServiceCallback callback) {
  std::move(request_bind_service_callback_)
      .Run(std::move(interface_name), std::move(receiver_pipe),
           std::move(callback));
}

void FakeCfmServiceContext::SetFakeProvideAdaptorCallback(
    FakeProvideAdaptorCallback callback) {
  provide_adaptor_callback_ = std::move(callback);
}

void FakeCfmServiceContext::SetFakeRequestBindServiceCallback(
    FakeRequestBindServiceCallback callback) {
  request_bind_service_callback_ = std::move(callback);
}

}  // namespace cfm
}  // namespace chromeos
