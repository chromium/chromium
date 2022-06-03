// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_handler_test_helper.h"

#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_test_helper_base.h"

namespace chromeos {

NetworkHandlerTestHelper::NetworkHandlerTestHelper() {
  if (!NetworkHandler::IsInitialized()) {
    NetworkHandler::Initialize();
    network_handler_initialized_ = true;
  }
}

NetworkHandlerTestHelper::~NetworkHandlerTestHelper() {
  if (network_handler_initialized_)
    NetworkHandler::Shutdown();
}

}  // namespace chromeos
