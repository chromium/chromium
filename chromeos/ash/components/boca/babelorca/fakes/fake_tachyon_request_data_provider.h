// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_REQUEST_DATA_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_REQUEST_DATA_PROVIDER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"

namespace ash::babelorca {

class FakeTachyonRequestDataProvider : public TachyonRequestDataProvider {
 public:
  FakeTachyonRequestDataProvider();
  FakeTachyonRequestDataProvider(std::optional<std::string> session_id,
                                 std::optional<std::string> tachyon_token,
                                 std::optional<std::string> group_id,
                                 std::optional<std::string> sender_email);

  FakeTachyonRequestDataProvider(const FakeTachyonRequestDataProvider&) =
      delete;
  FakeTachyonRequestDataProvider& operator=(
      const FakeTachyonRequestDataProvider&) = delete;

  ~FakeTachyonRequestDataProvider() override;

  // TachyonRequestDataProvider:
  void SigninToTachyonAndRespond(
      base::OnceCallback<void(bool)> on_response_cb) override;
  std::optional<std::string> session_id() const override;
  std::optional<std::string> tachyon_token() const override;
  std::optional<std::string> group_id() const override;
  std::optional<std::string> sender_email() const override;

  void set_tachyon_token(std::optional<std::string> tachyon_token);
  void set_group_id(std::optional<std::string> group_id);

  base::OnceCallback<void(bool)> TakeSigninCb();

 private:
  base::OnceCallback<void(bool)> signin_cb_;

  std::optional<std::string> session_id_;
  std::optional<std::string> tachyon_token_;
  std::optional<std::string> group_id_;
  std::optional<std::string> sender_email_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_REQUEST_DATA_PROVIDER_H_
