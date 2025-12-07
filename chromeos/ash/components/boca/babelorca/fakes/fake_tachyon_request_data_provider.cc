// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"

namespace ash::babelorca {

FakeTachyonRequestDataProvider::FakeTachyonRequestDataProvider() = default;

FakeTachyonRequestDataProvider::FakeTachyonRequestDataProvider(
    std::optional<std::string> session_id,
    std::optional<std::string> tachyon_token,
    std::optional<std::string> group_id,
    std::optional<std::string> sender_email)
    : session_id_(std::move(session_id)),
      tachyon_token_(std::move(tachyon_token)),
      group_id_(std::move(group_id)),
      sender_email_(std::move(sender_email)) {}

FakeTachyonRequestDataProvider::~FakeTachyonRequestDataProvider() = default;

void FakeTachyonRequestDataProvider::SigninToTachyonAndRespond(
    base::OnceCallback<void(bool)> on_response_cb) {
  signin_cb_ = std::move(on_response_cb);
}

std::optional<std::string> FakeTachyonRequestDataProvider::session_id() const {
  return session_id_;
}

std::optional<std::string> FakeTachyonRequestDataProvider::tachyon_token()
    const {
  return tachyon_token_;
}

std::optional<std::string> FakeTachyonRequestDataProvider::group_id() const {
  return group_id_;
}

std::optional<std::string> FakeTachyonRequestDataProvider::sender_email()
    const {
  return sender_email_;
}

void FakeTachyonRequestDataProvider::set_tachyon_token(
    std::optional<std::string> tachyon_token) {
  tachyon_token_ = tachyon_token;
}

void FakeTachyonRequestDataProvider::set_group_id(
    std::optional<std::string> group_id) {
  group_id_ = group_id;
}

base::OnceCallback<void(bool)> FakeTachyonRequestDataProvider::TakeSigninCb() {
  return std::move(signin_cb_);
}

}  // namespace ash::babelorca
