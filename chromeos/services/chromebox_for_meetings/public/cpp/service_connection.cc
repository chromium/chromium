// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"

namespace chromeos {
namespace cfm {

namespace {
static ServiceConnection* g_fake_service_connection_for_testing = nullptr;
}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection_for_testing) {
    return g_fake_service_connection_for_testing;
  }

  // The real impl lies in service_connection_ash.cc and
  // service_connection_lacros.cc.
  return GetInstanceForCurrentPlatform();
}

void ServiceConnection::UseFakeServiceConnectionForTesting(
    ServiceConnection* const fake_service_connection) {
  g_fake_service_connection_for_testing = fake_service_connection;
}

}  // namespace cfm
}  // namespace chromeos
