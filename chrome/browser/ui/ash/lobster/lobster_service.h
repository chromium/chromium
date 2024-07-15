// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_SERVICE_H_

#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/lobster/lobster_system_state_provider.h"
#include "components/keyed_service/core/keyed_service.h"

class LobsterService : public KeyedService {
 public:
  LobsterService();
  ~LobsterService() override;

  void SetActiveSession(ash::LobsterSession* session);

  ash::LobsterSession* active_session();
  LobsterSystemStateProvider* system_state_provider();

 private:
  // Not owned by this class
  raw_ptr<ash::LobsterSession> active_session_;

  LobsterSystemStateProvider system_state_provider_;
};

#endif  // CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_SERVICE_H_
