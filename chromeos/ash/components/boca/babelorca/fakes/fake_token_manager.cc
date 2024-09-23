// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"

namespace ash::babelorca {

FakeTokenManager::FakeTokenManager() = default;

FakeTokenManager::~FakeTokenManager() = default;

const std::string* FakeTokenManager::GetTokenString() {
  return token_string_.get();
}

int FakeTokenManager::GetFetchedVersion() {
  return version_;
}

void FakeTokenManager::ForceFetchToken(
    base::OnceCallback<void(bool)> success_callback) {
  success_callback_ = std::move(success_callback);
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void FakeTokenManager::SetTokenString(
    std::unique_ptr<std::string> token_string) {
  token_string_ = std::move(token_string);
}

void FakeTokenManager::SetFetchedVersion(int version) {
  version_ = version;
}

void FakeTokenManager::WaitForForceFetchRequest() {
  if (success_callback_) {
    return;
  }
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void FakeTokenManager::ExecuteFetchCallback(bool success) {
  return std::move(success_callback_).Run(success);
}

}  // namespace ash::babelorca
