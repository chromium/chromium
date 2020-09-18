// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONTROL_SERVICE_H_
#define CHROME_UPDATER_CONTROL_SERVICE_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace updater {

// The ControlService is the cross-platform abstraction for performing admin
// tasks on the updater.
class ControlService : public base::RefCountedThreadSafe<ControlService> {
 public:
  // Runs the ControlService and checks for updates if needed.
  virtual void Run(base::OnceClosure callback) = 0;

  // When ControlServiceOutOfProcess::InitializeUpdateService is invoked, the
  // server will wake and do its ModeCheck. As a result, the candidate can be
  // qualified and promoted (thus initializing the UpdateService for this
  // candidate). This is intended as a way for --install and --register to have
  // a way of ensuring there is an active updater on the system, without
  // performing expensive operations such as checking for updates.
  virtual void InitializeUpdateService(base::OnceClosure callback) = 0;

  // Provides a way to commit data or clean up resources before the task
  // scheduler is shutting down.
  virtual void Uninitialize() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ControlService>;

  virtual ~ControlService() = default;
};

// A factory method to create a ControlService class instance.
scoped_refptr<ControlService> CreateControlService();

}  // namespace updater

#endif  // CHROME_UPDATER_CONTROL_SERVICE_H_
