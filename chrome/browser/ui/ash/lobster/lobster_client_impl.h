// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_

#include "ash/public/cpp/lobster/lobster_client.h"

class LobsterClientImpl : public ash::LobsterClient {
 public:
  LobsterClientImpl();
  ~LobsterClientImpl() override;

  // LobsterClient overrides
  bool IsFeatureAllowed() override;
};

#endif  // CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_
