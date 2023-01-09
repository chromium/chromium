// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

UpdateServiceInternalImpl::UpdateServiceInternalImpl() = default;

void UpdateServiceInternalImpl::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  // A ref to service is retained by the callback.
  scoped_refptr<UpdateService> service =
      CreateUpdateServiceProxy(GetUpdaterScope());
  service->RunPeriodicTasks(base::BindOnce(
      [](base::OnceClosure callback, scoped_refptr<UpdateService> service) {
        std::move(callback).Run();
      },
      std::move(callback), service));
}

void UpdateServiceInternalImpl::Hello(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  std::move(callback).Run();
}

UpdateServiceInternalImpl::~UpdateServiceInternalImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
