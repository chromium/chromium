// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_READAHEAD_LOGIN_READAHEAD_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_READAHEAD_LOGIN_READAHEAD_PERFORMER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/synchronization/atomic_flag.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace ash {

// Wraps an AtomicFlag for notifying the readahead task when it should stop.
class CancelNotifier : public base::RefCountedThreadSafe<CancelNotifier> {
 public:
  CancelNotifier();
  void Cancel() { cancel_requested_.Set(); }
  bool IsCancelRequested() const { return cancel_requested_.IsSet(); }

 private:
  friend base::RefCountedThreadSafe<CancelNotifier>;
  ~CancelNotifier();

  base::AtomicFlag cancel_requested_;
};

// Perform readahead to speed up login. This spawns readahead tasks that are
// running while login prompt is displayed. Tasks are canceled when session is
// started to avoid conflicts between critical tasks for login.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_READAHEAD)
    LoginReadaheadPerformer : public SessionManagerClient::Observer {
 public:
  explicit LoginReadaheadPerformer(
      SessionManagerClient* session_manager_client);
  LoginReadaheadPerformer(const LoginReadaheadPerformer&) = delete;
  LoginReadaheadPerformer& operator=(const LoginReadaheadPerformer&) = delete;
  ~LoginReadaheadPerformer() override;

  // SessionManagerClient::Observer overrides:
  void EmitLoginPromptVisibleCalled() override;
  void StartSessionExCalled() override;

 private:
  scoped_refptr<CancelNotifier> cancel_notifier_;

  base::ScopedObservation<SessionManagerClient, SessionManagerClient::Observer>
      scoped_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_READAHEAD_LOGIN_READAHEAD_PERFORMER_H_
