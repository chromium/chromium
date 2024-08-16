// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_RESPONSE_CALLBACK_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_RESPONSE_CALLBACK_WRAPPER_H_

#include <string>

#include "base/types/expected.h"

namespace ash::babelorca {

class ResponseCallbackWrapper {
 public:
  enum class TachyonRequestError {
    kHttpError,
    kNetworkError,
    kInternalError,
    kAuthError
  };

  ResponseCallbackWrapper(const ResponseCallbackWrapper&) = delete;
  ResponseCallbackWrapper& operator=(const ResponseCallbackWrapper&) = delete;

  virtual ~ResponseCallbackWrapper() = default;

  virtual void Run(
      base::expected<std::string, TachyonRequestError> response) = 0;

 protected:
  ResponseCallbackWrapper() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_RESPONSE_CALLBACK_WRAPPER_H_
