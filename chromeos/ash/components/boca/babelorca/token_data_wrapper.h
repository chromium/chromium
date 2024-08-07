// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_DATA_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_DATA_WRAPPER_H_

#include <string>
#include <utility>

#include "base/time/time.h"

namespace ash::babelorca {

struct TokenDataWrapper {
  std::string token;
  base::Time expiration_time;

  TokenDataWrapper() = default;
  TokenDataWrapper(std::string token_param, base::Time expiration_time_param)
      : token(std::move(token_param)), expiration_time(expiration_time_param) {}
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TOKEN_DATA_WRAPPER_H_
