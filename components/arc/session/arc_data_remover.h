// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_DATA_REMOVER_H_
#define COMPONENTS_ARC_SESSION_ARC_DATA_REMOVER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/concierge_client.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace arc {

// Manages ARC's user data removal operation.
class ArcDataRemover {
 public:
  ArcDataRemover(PrefService* prefs,
                 const cryptohome::Identification& cryptohome_id);
  ~ArcDataRemover();

  // Schedules to remove the data. This is persistent, calling Run() just
  // after rebooting may execute the removing.
  void Schedule();

  // Returns whether data removal is scheduled or not for testing purpose.
  bool IsScheduledForTesting() const;

  // Executes the removing, if scheduled.
  // This must run while ARC instance is stopped.
  // If not scheduled, |callback| will be synchronously called with nullopt.
  using RunCallback = base::OnceCallback<void(base::Optional<bool> result)>;
  void Run(RunCallback callback);

  // TODO(yusukes): Remove the setter.
  void set_user_id_hash_for_profile(const std::string& user_id_hash) {
    user_id_hash_ = user_id_hash;
  }

 private:
  void OnDataRemoved(RunCallback callback, bool success);

  // TODO(yusukes): Remove these ARCVM functions.
  void OnConciergeStarted(RunCallback callback, bool success);
  void OnDiskImageDestroyed(
      RunCallback callback,
      base::Optional<vm_tools::concierge::DestroyDiskImageResponse> reply);

  THREAD_CHECKER(thread_checker_);

  // Pref accessor to the "arc.data.remove_requested".
  BooleanPrefMember pref_;

  // Cryptohome ID for the user whose /data is being deleted.
  const cryptohome::Identification cryptohome_id_;

  // TODO(yusukes): Remove the member variable.
  std::string user_id_hash_;

  base::WeakPtrFactory<ArcDataRemover> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArcDataRemover);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_DATA_REMOVER_H_
