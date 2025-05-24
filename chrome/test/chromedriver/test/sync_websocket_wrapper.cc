// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/test/sync_websocket_wrapper.h"

#include "base/compiler_specific.h"

SyncWebSocketWrapper::SyncWebSocketWrapper(
    std::unique_ptr<SyncWebSocket> wrapped_socket)
    : wrapped_socket_(std::move(wrapped_socket)) {}

SyncWebSocketWrapper::~SyncWebSocketWrapper() = default;

void SyncWebSocketWrapper::SetId(const std::string& socket_id) {
  wrapped_socket_->SetId(socket_id);
}

bool SyncWebSocketWrapper::IsConnected() {
  return wrapped_socket_->IsConnected();
}

bool SyncWebSocketWrapper::Connect(const GURL& url) {
  return wrapped_socket_->Connect(url);
}

bool SyncWebSocketWrapper::Send(const std::string& message) {
  return wrapped_socket_->Send(message);
}

SyncWebSocket::StatusCode SyncWebSocketWrapper::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  return wrapped_socket_->ReceiveNextMessage(message, timeout);
}

bool SyncWebSocketWrapper::HasNextMessage() {
  return wrapped_socket_->HasNextMessage();
}

void SyncWebSocketWrapper::SetNotificationCallback(
    base::RepeatingClosure callback) {
  wrapped_socket_->SetNotificationCallback(std::move(callback));
}
