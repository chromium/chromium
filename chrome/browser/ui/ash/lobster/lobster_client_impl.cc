// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/lobster_client_impl.h"

LobsterClientImpl::LobsterClientImpl() = default;

LobsterClientImpl::~LobsterClientImpl() = default;

bool LobsterClientImpl::IsFeatureAllowed() {
  // TODO(b/348280621): Implement enable / disable module
  return false;
}
