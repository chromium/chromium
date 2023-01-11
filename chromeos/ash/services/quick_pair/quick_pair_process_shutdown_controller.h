// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_SHUTDOWN_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_SHUTDOWN_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/timer/timer.h"

namespace ash {
namespace quick_pair {

// Controls the shutdown logic for the Quick Pair utility process once it is no
// longer being used.
class QuickPairProcessShutdownController {
 public:
  QuickPairProcessShutdownController();
  QuickPairProcessShutdownController(
      const QuickPairProcessShutdownController&) = delete;
  QuickPairProcessShutdownController& operator=(
      const QuickPairProcessShutdownController&);
  virtual ~QuickPairProcessShutdownController();

  // Start the shutdown process. |callback| will be invoked once this class has
  // determined that the process should be shutdown.
  virtual void Start(base::OnceClosure callback);

  // Stop the shutdown process. This should be called when the client knows
  // the process should no longer be shutdown, e.g. when a new reference to it
  // is created.
  virtual void Stop();

 private:
  base::OneShotTimer timer_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_SHUTDOWN_CONTROLLER_H_
