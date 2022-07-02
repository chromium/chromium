// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_UTILS_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_UTILS_H_

#include "base/callback.h"

namespace chromeos {
namespace cros_healthd {
namespace connectivity {

// Runs or returns. Gets the result of the callback of |get_result|. If the
// result is true, runs the |run_callback| and passes the |return_callback| as
// argument. If false, runs the |return_callback| with the |return_value|.
// The blocking version of this is:
//   if (!get_result())
//      return return_value;
//   // keep running.
void RunOrReturn(
    bool return_value,
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> get_result,
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> run_callback,
    base::OnceCallback<void(bool)> return_callback);

// Runs |on_success| or |on_failed| based on the result of the callback of
// |get_result|.
// The blocking version of this is:
//   if (get_result()) {
//     // on success.
//   } else {
//     // on failed;
//   }
void RunSuccessOrFailed(
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> get_result,
    base::OnceClosure on_success,
    base::OnceClosure on_failed);

}  // namespace connectivity
}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_UTILS_H_
