// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/api_bindings/scoped_api_binding.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/cast/api_bindings/manager.h"

namespace cast_api_bindings {

ScopedApiBinding::ScopedApiBinding(Manager* bindings_manager,
                                   Delegate* delegate,
                                   std::string_view js_bindings_id,
                                   std::string_view js_bindings)
    : bindings_manager_(bindings_manager),
      delegate_(delegate),
      js_bindings_id_(js_bindings_id) {
  DCHECK(bindings_manager_);
  DCHECK(!js_bindings_id_.empty());

  bindings_manager_->AddBinding(js_bindings_id_, js_bindings);

  if (delegate_) {
    bindings_manager_->RegisterPortHandler(
        delegate_->GetPortName(),
        base::BindRepeating(&ScopedApiBinding::OnPortConnected,
                            base::Unretained(this)));
  }
}

ScopedApiBinding::~ScopedApiBinding() {
  // TODO(crbug.com/40139651): Remove binding JS when RemoveBinding() added to
  // ApiBindingsManager.

  if (delegate_) {
    bindings_manager_->UnregisterPortHandler(delegate_->GetPortName());
  }
}

void ScopedApiBinding::OnPortConnected(
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  message_port_ = std::move(port);
  message_port_->SetReceiver(this);
  delegate_->OnConnected();
}

bool ScopedApiBinding::SendMessage(std::string_view data_utf8) {
  DCHECK(delegate_);

  DVLOG(1) << "SendMessage: message=" << data_utf8;
  if (!message_port_->CanPostMessage()) {
    LOG(WARNING)
        << "Attempted to write to unconnected MessagePort, dropping message.";
    return false;
  }

  if (!message_port_->PostMessage(data_utf8)) {
    return false;
  }

  return true;
}

bool ScopedApiBinding::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  return delegate_->OnMessage(message);
}

void ScopedApiBinding::OnPipeError() {
  delegate_->OnDisconnected();
}

}  // namespace cast_api_bindings
