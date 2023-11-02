// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash::assistant {

namespace {

AssistantService* g_instance = nullptr;

}  // namespace

// static
AssistantService* AssistantService::Get() {
  return g_instance;
}

AssistantService::AssistantService() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AssistantService::~AssistantService() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash::assistant
