// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

// Returns a random `TimeDelta` value used to delay the start of the automated
// background tasks such as update checks. This distributes the update server
// load more uniformly and avoids the problem of a large number of clients
// creating load spikes on servers when checking for updates and their system
// time is synchronized by a time server.
base::TimeDelta UpdateCheckJitter();

class Configurator;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInternalImpl : public UpdateServiceInternal {
 public:
  explicit UpdateServiceInternalImpl(
      scoped_refptr<updater::Configurator> config);

  // Overrides for updater::UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void InitializeUpdateService(base::OnceClosure callback) override;

  void Uninitialize() override;

  class Task : public base::RefCountedThreadSafe<Task> {
   public:
    virtual void Run() = 0;

   protected:
    friend class base::RefCountedThreadSafe<Task>;

    virtual ~Task() = default;
  };

  // Callback to run after a `Task` has finished.
  void TaskDone(base::OnceClosure callback);

 private:
  ~UpdateServiceInternalImpl() override;

  void RunNextTask();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<updater::Configurator> config_;

  // The queue prevents multiple Task instances from running simultaneously and
  // processes them sequentially.
  base::queue<scoped_refptr<Task>> tasks_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_H_
