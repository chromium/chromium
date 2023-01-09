// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace updater {

enum class UpdaterScope;

// The UpdateServiceInternal is a service abstraction to expose functionality
// made available only to callers which are part of the same instance of
// the updater installation. In other words, only a client and a service with
// identical build versions can communicate using this interface.
class UpdateServiceInternal
    : public base::RefCountedThreadSafe<UpdateServiceInternal> {
 public:
  // Runs the UpdateServiceInternal and checks for updates if needed.
  virtual void Run(base::OnceClosure callback) = 0;

  // When UpdateServiceInternalProxy::Hello is invoked, the server will wake and
  // do its ModeCheck. As a result, the candidate can be qualified and promoted
  // (thus initializing the UpdateService for this candidate). Calling this
  // function ensures that there is an active updater on the system when
  // --install is running without performing expensive operations such as
  // checking for updates.
  virtual void Hello(base::OnceClosure callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateServiceInternal>;

  virtual ~UpdateServiceInternal() = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_H_
