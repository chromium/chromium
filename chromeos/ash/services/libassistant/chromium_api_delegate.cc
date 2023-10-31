// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/chromium_api_delegate.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::libassistant {

ChromiumApiDelegate::ChromiumApiDelegate(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory)
    : http_connection_factory_(std::move(pending_url_loader_factory)) {}

ChromiumApiDelegate::~ChromiumApiDelegate() = default;

assistant_client::HttpConnectionFactory*
ChromiumApiDelegate::GetHttpConnectionFactory() {
  return &http_connection_factory_;
}

}  // namespace ash::libassistant
