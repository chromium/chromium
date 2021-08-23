// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/projector_app_client.h"

#include "base/check_op.h"

namespace chromeos {

namespace {
ProjectorAppClient* g_instance = nullptr;
}  // namespace

// static
ProjectorAppClient* ProjectorAppClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

ProjectorAppClient::ProjectorAppClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

ProjectorAppClient::~ProjectorAppClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace chromeos
