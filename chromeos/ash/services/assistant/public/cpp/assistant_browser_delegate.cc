// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"

namespace ash::assistant {

namespace {

AssistantBrowserDelegate* g_instance = nullptr;

}  // namespace

// static
AssistantBrowserDelegate* AssistantBrowserDelegate::Get() {
  DCHECK(g_instance);
  return g_instance;
}

AssistantBrowserDelegate::AssistantBrowserDelegate() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AssistantBrowserDelegate::~AssistantBrowserDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash::assistant
