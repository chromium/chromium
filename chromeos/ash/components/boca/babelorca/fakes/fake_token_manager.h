// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TOKEN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TOKEN_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"

namespace ash::babelorca {

class FakeTokenManager : public TokenManager {
 public:
  FakeTokenManager();

  FakeTokenManager(const FakeTokenManager&) = delete;
  FakeTokenManager& operator=(const FakeTokenManager&) = delete;

  ~FakeTokenManager() override;

  // TokenManager:
  const std::string* GetTokenString() override;
  int GetFetchedVersion() override;
  void ForceFetchToken(
      base::OnceCallback<void(bool)> success_callback) override;

  void SetTokenString(std::unique_ptr<std::string> token_string);
  void SetFetchedVersion(int version);
  void WaitForForceFetchRequest();
  void ExecuteFetchCallback(bool success);

 private:
  std::unique_ptr<std::string> token_string_;
  int version_ = 0;
  base::OnceCallback<void(bool)> success_callback_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TOKEN_MANAGER_H_
