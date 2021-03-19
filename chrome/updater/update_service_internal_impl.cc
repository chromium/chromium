// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/util.h"

namespace updater {

UpdateServiceInternalImpl::UpdateServiceInternalImpl() = default;

void UpdateServiceInternalImpl::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // A ref to service is retained by the callback.
  scoped_refptr<UpdateService> service = CreateUpdateService();
  service->RunPeriodicTasks(base::BindOnce(
      [](base::OnceClosure callback, scoped_refptr<UpdateService> service) {
        std::move(callback).Run();
      },
      std::move(callback), service));
}

void UpdateServiceInternalImpl::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void UpdateServiceInternalImpl::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

UpdateServiceInternalImpl::~UpdateServiceInternalImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
