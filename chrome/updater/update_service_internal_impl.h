// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInternalImpl : public UpdateServiceInternal {
 public:
  UpdateServiceInternalImpl();

  // Overrides for updater::UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void Hello(base::OnceClosure callback) override;

 private:
  ~UpdateServiceInternalImpl() override;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_
