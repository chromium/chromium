// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/sync/engine/net/http_post_provider_factory.h"

namespace fake_server {

class FakeServer;

syncer::CreateHttpPostProviderFactory CreateFakeServerHttpPostProviderFactory(
    const base::WeakPtr<FakeServer>& fake_server);

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_
