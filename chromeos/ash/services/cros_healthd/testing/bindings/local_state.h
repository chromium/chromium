// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_LOCAL_STATE_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_LOCAL_STATE_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"

#include "chromeos/ash/services/cros_healthd/testing/bindings/mojom/state.mojom.h"

namespace ash::cros_healthd::connectivity {

// LocalState provides interface to set the local internal state of
// connectivity test between two context object in each processes.
class LocalState {
 public:
  LocalState(const LocalState&) = delete;
  LocalState& operator=(const LocalState&) = delete;
  virtual ~LocalState() = default;

  static std::unique_ptr<LocalState> Create(
      mojo::PendingReceiver<mojom::State> receiver);

 public:
  // Used by TestProvider to set the LastCallHasNext state.
  // LastCallHasNext reports whether there are more response parameters to be
  // tested from the function last called. Only the function with response
  // parameters sets this state.
  virtual void SetLastCallHasNext(bool has_next) = 0;
  // Used by TestProvider to fulfill the callback set by remote's
  // |WaitRemoteLastCall|. Call this before the target function returns to
  // notify remote that the function is finished.
  virtual void FulfillLastCallCallback() = 0;

 protected:
  LocalState() = default;
};

}  // namespace ash::cros_healthd::connectivity

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_LOCAL_STATE_H_
