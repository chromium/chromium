// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test/ios_components_test_initializer.h"

#include "services/network/test/test_network_connection_tracker.h"

IosComponentsTestInitializer::IosComponentsTestInitializer()
    : network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()) {}

IosComponentsTestInitializer::~IosComponentsTestInitializer() = default;
