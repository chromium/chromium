// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"

#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"

namespace chromeos::cfm {

void ServiceAdaptor::Delegate::OnAdaptorConnect(bool success) {}

void ServiceAdaptor::Delegate::OnAdaptorDisconnect() {}

ServiceAdaptor::ServiceAdaptor(std::string interface_name, Delegate* delegate)
    : interface_name_(std::move(interface_name)), delegate_(delegate) {
  DCHECK(delegate_);
}

ServiceAdaptor::~ServiceAdaptor() = default;

mojom::CfmServiceContext* ServiceAdaptor::GetContext() {
  if (!context_.is_bound()) {
    ServiceConnection::GetInstance()->BindServiceContext(
        context_.BindNewPipeAndPassReceiver());
    context_.reset_on_disconnect();
  }

  return context_.get();
}

void ServiceAdaptor::GetService(std::string interface_name,
                                mojo::ScopedMessagePipeHandle receiver_pipe,
                                GetServiceCallback callback) {
  GetContext()->RequestBindService(
      std::move(interface_name), std::move(receiver_pipe), std::move(callback));
}

void ServiceAdaptor::BindServiceAdaptor() {
  if (adaptor_.is_bound()) {
    return;
  }

  GetContext()->ProvideAdaptor(interface_name_,
                               adaptor_.BindNewPipeAndPassRemote(),
                               base::BindOnce(&ServiceAdaptor::OnAdaptorConnect,
                                              weak_ptr_factory_.GetWeakPtr()));

  adaptor_.set_disconnect_handler(base::BindOnce(
      &ServiceAdaptor::OnAdaptorDisconnect, base::Unretained(this)));
}

void ServiceAdaptor::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  delegate_->OnBindService(std::move(receiver_pipe));
}

void ServiceAdaptor::OnAdaptorConnect(bool success) {
  DLOG_IF(WARNING, !success) << "Failed Registration for " << interface_name_;
  if (!success) {
    // If the connection to |CfmServiceContext| is unsuccessful reset the
    // adaptor to allow for future attempts.
    adaptor_.reset();
  }

  delegate_->OnAdaptorConnect(success);
}

void ServiceAdaptor::OnAdaptorDisconnect() {
  adaptor_.reset();

  delegate_->OnAdaptorDisconnect();
}

}  // namespace chromeos::cfm
