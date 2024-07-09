// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_SERVICE_H_

#include "chrome/browser/ui/ash/lobster/lobster_system_state_provider.h"
#include "components/keyed_service/core/keyed_service.h"

class LobsterService : public KeyedService {
 public:
  LobsterService();
  ~LobsterService() override;

  LobsterSystemStateProvider* system_state_provider();

 private:
  LobsterSystemStateProvider system_state_provider_;
};

#endif  // CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_SERVICE_H_
