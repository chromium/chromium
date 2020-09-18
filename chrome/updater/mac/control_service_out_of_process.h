// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_CONTROL_SERVICE_OUT_OF_PROCESS_H_
#define CHROME_UPDATER_MAC_CONTROL_SERVICE_OUT_OF_PROCESS_H_

#import <Foundation/Foundation.h>

#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/control_service.h"
#include "chrome/updater/service_scope.h"

@class CRUControlServiceOutOfProcessImpl;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace updater {

// All functions and callbacks must be called on the same sequence.
class ControlServiceOutOfProcess : public ControlService {
 public:
  explicit ControlServiceOutOfProcess(ServiceScope scope);

  // Overrides for ControlService.
  void Run(base::OnceClosure callback) override;
  void InitializeUpdateService(base::OnceClosure callback) override;
  void Uninitialize() override;

 private:
  ~ControlServiceOutOfProcess() override;

  SEQUENCE_CHECKER(sequence_checker_);

  base::scoped_nsobject<CRUControlServiceOutOfProcessImpl> client_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_CONTROL_SERVICE_OUT_OF_PROCESS_H_
