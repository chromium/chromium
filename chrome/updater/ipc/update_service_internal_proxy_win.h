// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_WIN_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

enum class UpdaterScope;
class UpdateServiceInternalProxyImpl;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInternalProxy : public UpdateServiceInternal {
 public:
  explicit UpdateServiceInternalProxy(UpdaterScope scope);

  // Overrides for UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void Hello(base::OnceClosure callback) override;

 private:
  ~UpdateServiceInternalProxy() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceInternalProxyImpl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_WIN_H_
