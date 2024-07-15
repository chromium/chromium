// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/lobster/lobster_service.h"
#include "chrome/browser/ui/ash/lobster/lobster_system_state_provider.h"

namespace ash {
struct LobsterSystemState;
}  // namespace ash

class LobsterClientImpl : public ash::LobsterClient {
 public:
  explicit LobsterClientImpl(LobsterService* service);
  ~LobsterClientImpl() override;

  // LobsterClient overrides
  void SetActiveSession(ash::LobsterSession* session) override;
  ash::LobsterSystemState GetSystemState() override;

 private:
  // Not owned by this class
  raw_ptr<LobsterService> service_;
};

#endif  // CHROME_BROWSER_UI_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_
