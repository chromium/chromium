// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/management_disclosure/management_disclosure_client_impl.h"

#include "ash/public/cpp/login_screen.h"

namespace {
ManagementDisclosureClientImpl* g_management_disclosure_client_instance =
    nullptr;
}  // namespace

ManagementDisclosureClientImpl::ManagementDisclosureClientImpl() {
  // Register this object as the client interface implementation.
  ash::LoginScreen::Get()->SetManagementDisclosureClient(this);

  DCHECK(!g_management_disclosure_client_instance);
  g_management_disclosure_client_instance = this;
}

ManagementDisclosureClientImpl::~ManagementDisclosureClientImpl() {
  // Register this object as the client interface implementation.
  ash::LoginScreen::Get()->SetManagementDisclosureClient(nullptr);

  DCHECK_EQ(this, g_management_disclosure_client_instance);
  g_management_disclosure_client_instance = nullptr;
}

void ManagementDisclosureClientImpl::SetVisible(bool visible) {}
