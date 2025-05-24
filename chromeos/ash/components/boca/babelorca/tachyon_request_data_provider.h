// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_DATA_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_DATA_PROVIDER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"

namespace ash::babelorca {

// Provider for tachyon requests common data.
class TachyonRequestDataProvider {
 public:
  TachyonRequestDataProvider(const TachyonRequestDataProvider&) = delete;
  TachyonRequestDataProvider& operator=(const TachyonRequestDataProvider&) =
      delete;

  virtual ~TachyonRequestDataProvider() = default;

  // Signin to Tachyon and run the callback with `true` on success and with
  // `false` otherwise.
  virtual void SigninToTachyonAndRespond(
      base::OnceCallback<void(bool)> on_response_cb) = 0;

  virtual std::optional<std::string> session_id() const = 0;
  virtual std::optional<std::string> tachyon_token() const = 0;
  virtual std::optional<std::string> group_id() const = 0;
  virtual std::optional<std::string> sender_email() const = 0;

 protected:
  TachyonRequestDataProvider() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_DATA_PROVIDER_H_
