// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/mock_auth_status_consumer.h"

#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

MockAuthStatusConsumer::MockAuthStatusConsumer(base::OnceClosure quit_closure)
    : quit_closure_(std::move(quit_closure)) {}

MockAuthStatusConsumer::~MockAuthStatusConsumer() = default;

void MockAuthStatusConsumer::OnRetailModeSuccessQuit(
    const UserContext& user_context) {
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnRetailModeSuccessQuitAndFail(
    const UserContext& user_context) {
  ADD_FAILURE() << "Retail mode login should have failed!";
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnGuestSuccessQuit() {
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnGuestSuccessQuitAndFail() {
  ADD_FAILURE() << "Guest login should have failed!";
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnSuccessQuit(const UserContext& user_context) {
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnSuccessQuitAndFail(
    const UserContext& user_context) {
  ADD_FAILURE() << "Login should NOT have succeeded!";
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnFailQuit(const AuthFailure& error) {
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnFailQuitAndFail(const AuthFailure& error) {
  ADD_FAILURE() << "Login should not have failed!";
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnMigrateQuit() {
  std::move(quit_closure_).Run();
}

void MockAuthStatusConsumer::OnMigrateQuitAndFail() {
  ADD_FAILURE() << "Should not have detected a PW change!";
  std::move(quit_closure_).Run();
}

}  // namespace ash
