// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/test_support/mock_preview_server_proxy.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace data_sharing {

MockPreviewServerProxy::MockPreviewServerProxy()
    : PreviewServerProxy(nullptr, nullptr, version_info::Channel::UNKNOWN) {}

MockPreviewServerProxy::~MockPreviewServerProxy() = default;

}  // namespace data_sharing
