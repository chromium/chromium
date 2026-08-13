// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_app_client.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/version_info/channel.h"
#include "chromeos/ash/components/boca/util.h"
#include "chromeos/ash/components/channel/channel_info.h"

namespace ash::boca {

namespace {

inline constexpr char kDummyDeviceId[] = "kDummyDeviceId";
// Non thread safe, life cycle is managed by owner.
BocaAppClient* g_instance = nullptr;

}  // namespace

// static
BocaAppClient* BocaAppClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

bool BocaAppClient::HasInstance() {
  return g_instance;
}

BocaAppClient::BocaAppClient() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

BocaAppClient::~BocaAppClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BocaAppClient::LaunchApp() {}

int BocaAppClient::GetAppInstanceCount() {
  return 0;
}

std::string BocaAppClient::GetDeviceId() {
  return kDummyDeviceId;
}

std::string BocaAppClient::GetSchoolToolsServerBaseUrl() {
  return GetSchoolToolsUrl();
}

// Implemented in boca_app_client_impl.cc
void BocaAppClient::OpenFeedbackDialog() {}

std::unique_ptr<SharedCrdSessionWrapper>
BocaAppClient::CreateSharedCrdSessionWrapper() {
  return nullptr;
}

}  // namespace ash::boca
