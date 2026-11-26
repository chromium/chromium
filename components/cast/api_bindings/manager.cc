// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/api_bindings/manager.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace cast_api_bindings {

class Manager::DisconnectableMessagePort
    : public cast_api_bindings::MessagePort,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  DisconnectableMessagePort(
      base::WeakPtr<Manager> manager,
      std::unique_ptr<cast_api_bindings::MessagePort> port)
      : manager_(std::move(manager)), port_(std::move(port)) {
    DCHECK(port_);
  }

  ~DisconnectableMessagePort() override = default;

  DisconnectableMessagePort(const DisconnectableMessagePort&) = delete;
  DisconnectableMessagePort& operator=(const DisconnectableMessagePort&) =
      delete;

  base::WeakPtr<DisconnectableMessagePort> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::unique_ptr<cast_api_bindings::MessagePort> TakePort() {
    if (!port_ || receiver_) {
      return nullptr;
    }
    return std::move(port_);
  }

  void Disconnect() {
    if (!port_) {
      return;
    }
    std::unique_ptr<cast_api_bindings::MessagePort> port = std::move(port_);
    port->Close();
    port.reset();
    if (receiver_) {
      MessagePort::Receiver* receiver = std::exchange(receiver_, nullptr);
      receiver->OnPipeError();
    }
  }

  // cast_api_bindings::MessagePort implementation:
  bool PostMessage(std::string_view message) override {
    return port_ && port_->PostMessage(message);
  }

  bool PostMessageWithTransferables(
      std::string_view message,
      std::vector<std::unique_ptr<MessagePort>> ports) override {
    if (!port_) {
      return false;
    }
    if (!ports.empty()) {
      if (!manager_) {
        return false;
      }
      for (std::unique_ptr<MessagePort>& port : ports) {
        if (!port) {
          return false;
        }
        port = manager_->UnwrapPort(std::move(port));
        if (!port) {
          return false;
        }
      }
    }
    return port_->PostMessageWithTransferables(message, std::move(ports));
  }

  void SetReceiver(
      cast_api_bindings::MessagePort::Receiver* receiver) override {
    DCHECK(receiver);
    DCHECK(!receiver_);
    receiver_ = receiver;
    if (port_) {
      port_->SetReceiver(this);
    }
  }

  void Close() override {
    if (port_) {
      port_->Close();
      port_.reset();
    }
  }

  bool CanPostMessage() const override {
    return port_ && port_->CanPostMessage();
  }

 private:
  // cast_api_bindings::MessagePort::Receiver implementation:
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<MessagePort>> ports) override {
    if (!receiver_) {
      return false;
    }
    if (!ports.empty() && !manager_) {
      return false;
    }
    for (std::unique_ptr<MessagePort>& port : ports) {
      if (port) {
        port = manager_->WrapPort(std::move(port));
      }
    }
    return receiver_->OnMessage(message, std::move(ports));
  }

  void OnPipeError() override {
    port_.reset();
    if (receiver_) {
      MessagePort::Receiver* receiver = std::exchange(receiver_, nullptr);
      receiver->OnPipeError();
    }
  }

  base::WeakPtr<Manager> manager_;
  std::unique_ptr<cast_api_bindings::MessagePort> port_;
  raw_ptr<cast_api_bindings::MessagePort::Receiver> receiver_ = nullptr;
  base::WeakPtrFactory<DisconnectableMessagePort> weak_factory_{this};
};

Manager::Manager() = default;

Manager::~Manager() {
  DCHECK(port_handlers_.empty());
}

void Manager::RegisterPortHandler(std::string_view port_name,
                                  MessagePortConnectedHandler handler) {
  auto result = port_handlers_.try_emplace(port_name, std::move(handler));
  DCHECK(result.second);
}

void Manager::UnregisterPortHandler(std::string_view port_name) {
  size_t deleted = port_handlers_.erase(port_name);
  DCHECK_EQ(deleted, 1u);
}

std::unique_ptr<cast_api_bindings::MessagePort> Manager::WrapPort(
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  if (!port) {
    return nullptr;
  }
  std::erase_if(connected_ports_, [](const auto& entry) { return !entry; });
  auto disconnectable_port = std::make_unique<DisconnectableMessagePort>(
      weak_factory_.GetWeakPtr(), std::move(port));
  connected_ports_.push_back(disconnectable_port->GetWeakPtr());
  return disconnectable_port;
}

std::unique_ptr<cast_api_bindings::MessagePort> Manager::UnwrapPort(
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  if (!port) {
    return nullptr;
  }
  for (auto it = connected_ports_.begin(); it != connected_ports_.end(); ++it) {
    if (*it && static_cast<const cast_api_bindings::MessagePort*>(it->get()) ==
                   port.get()) {
      std::unique_ptr<cast_api_bindings::MessagePort> unwrapped =
          (*it)->TakePort();
      connected_ports_.erase(it);
      return unwrapped;
    }
  }
  return port;
}

bool Manager::OnPortConnected(
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  if (!port) {
    return false;
  }

  auto handler = port_handlers_.find(port_name);
  if (handler == port_handlers_.end()) {
    LOG(ERROR) << "No handler found for port " << port_name << ".";
    return false;
  }

  handler->second.Run(WrapPort(std::move(port)));
  return true;
}

void Manager::SetOrigin(std::string origin) {
  if (!origin.empty() && origin == origin_) {
    return;
  }
  origin_ = std::move(origin);
  std::erase_if(connected_ports_, [](const auto& entry) { return !entry; });
  std::vector<base::WeakPtr<DisconnectableMessagePort>> ports =
      connected_ports_;
  for (base::WeakPtr<DisconnectableMessagePort>& port : ports) {
    if (port) {
      port->Disconnect();
    }
  }
}

}  // namespace cast_api_bindings
