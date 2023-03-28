// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_UPDATE_SERVICE_INTERNAL_STUB_WIN_H_
#define CHROME_UPDATER_APP_SERVER_WIN_UPDATE_SERVICE_INTERNAL_STUB_WIN_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

// Receives calls from the client and delegates them to an UpdateServiceInternal
// implementation. Before each call to the UpdateServiceInternal implementation
// is invoked, `task_start_listener` is called. And after each call to the
// UpdateServiceInternal implementation has completed, `task_end_listener` is
// called.
class UpdateServiceInternalStubWin : public UpdateServiceInternal {
 public:
  // Create an UpdateServiceInternalStubWin which forwards calls to `impl`.
  UpdateServiceInternalStubWin(
      scoped_refptr<updater::UpdateServiceInternal> impl,
      base::RepeatingClosure task_start_listener,
      base::RepeatingClosure task_end_listener);
  UpdateServiceInternalStubWin(const UpdateServiceInternalStubWin&) = delete;
  UpdateServiceInternalStubWin& operator=(const UpdateServiceInternalStubWin&) =
      delete;

  // updater::UpdateServiceInternal overrides.
  void Run(base::OnceClosure callback) override;
  void Hello(base::OnceClosure callback) override;

 private:
  ~UpdateServiceInternalStubWin() override;

  scoped_refptr<updater::UpdateServiceInternal> impl_;
  base::RepeatingClosure task_start_listener_;
  base::RepeatingClosure task_end_listener_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_UPDATE_SERVICE_INTERNAL_STUB_WIN_H_
