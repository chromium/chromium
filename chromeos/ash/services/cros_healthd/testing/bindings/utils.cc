// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/testing/bindings/utils.h"

#include <utility>

#include "base/bind.h"

namespace chromeos {
namespace cros_healthd {
namespace connectivity {
namespace {

void RunOrReturnCallback(
    bool return_value,
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> run_callback,
    base::OnceCallback<void(bool)> return_callback,
    bool result) {
  if (result) {
    std::move(run_callback).Run(std::move(return_callback));
  } else {
    std::move(return_callback).Run(return_value);
  }
}

void RunSuccessOrFailedCallback(base::OnceClosure on_success,
                                base::OnceClosure on_failed,
                                bool result) {
  if (result) {
    std::move(on_success).Run();
  } else {
    std::move(on_failed).Run();
  }
}
}  // namespace

void RunOrReturn(
    bool return_value,
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> get_result,
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> run_callback,
    base::OnceCallback<void(bool)> return_callback) {
  std::move(get_result)
      .Run(base::BindOnce(&RunOrReturnCallback, return_value,
                          std::move(run_callback), std::move(return_callback)));
}

void RunSuccessOrFailed(
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> get_result,
    base::OnceClosure on_success,
    base::OnceClosure on_failed) {
  std::move(get_result)
      .Run(base::BindOnce(&RunSuccessOrFailedCallback, std::move(on_success),
                          std::move(on_failed)));
}

}  // namespace connectivity
}  // namespace cros_healthd
}  // namespace chromeos
