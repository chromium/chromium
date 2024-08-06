// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DRM_KEY_TRACKER_H_
#define CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DRM_KEY_TRACKER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace chromecast {
namespace media {

// Tracks available DRM keys.
//
// This class should be accessed via the singleton returned from GetInstance.
// All its functions are threadsafe, so it can be accessed from any sequence.
//
// The CDM is expected to add and remove keys as those keys become
// available/unavailable. Decoders are expected to check whether keys are
// available (via HasKey) before pushing buffers to starboard. If necessary, the
// decoders can register callbacks to run when a given key is available (via
// WaitForKey).
class StarboardDrmKeyTracker {
 public:
  // A callback that takes an int64 ticket as an identifier. This will match the
  // ticket returned from WaitForKey.
  using KeyAvailableCb = base::OnceCallback<void(int64_t)>;

  // Returns the singleton instance of this class.
  static StarboardDrmKeyTracker& GetInstance();

  // Disallow copy and assign. This class should only be accessed via
  // GetInstance().
  StarboardDrmKeyTracker(const StarboardDrmKeyTracker&) = delete;
  StarboardDrmKeyTracker& operator=(const StarboardDrmKeyTracker&) = delete;

  // Adds a DRM key for the given session.
  void AddKey(const std::string& key, const std::string& session_id);

  // Removes a DRM key from the given session.
  void RemoveKey(const std::string& key, const std::string& session_id);

  // Removes all keys for the given session.
  void RemoveKeysForSession(const std::string& session_id);

  // Returns true if the DRM key exists in any session.
  bool HasKey(const std::string& key);

  // TODO(antoniori): we should probably require that the callback be run via a
  // posted task by taking a TaskRunner argument and posting the task in this
  // class. This works fine for now, but the TaskRunner approach would be
  // slightly safer.
  //
  // Registers `cb` to be run once `key` is available in a session. If it is
  // already available, `cb` will be run immediately. The argument to `cb` is
  // the token returned from WaitForKey.
  //
  // If the callback must run on a particular sequence, consider using
  // base::BindPostTask to create the callback. This class makes no guarantees
  // about which sequence will run `cb`.
  //
  // Returns an opaque token that can be used to unregister the callback at a
  // later time via UnregisterCallback, if necessary (e.g. when a session is
  // ending).
  int64_t WaitForKey(const std::string& key, KeyAvailableCb cb);

  // Unregisters a callback. `callback_token` should be the value previously
  // returned by WaitForKey.
  //
  // If the callback was already run or if the token is invalid, this is
  // functionally a no-op.
  void UnregisterCallback(int64_t callback_token);

  // Clears any state. Test code need to clear the instance's state so that
  // TESTs do not influence each other.
  void ClearStateForTesting();

 private:
  friend class base::NoDestructor<StarboardDrmKeyTracker>;

  // Holds a token and a callback, so that callbacks can be identified.
  struct TokenAndCallback {
    TokenAndCallback(int64_t token, KeyAvailableCb callback);

    // Movable, but not copyable.
    TokenAndCallback(TokenAndCallback&&);
    TokenAndCallback& operator=(TokenAndCallback&&);
    TokenAndCallback(TokenAndCallback&) = delete;
    TokenAndCallback& operator=(TokenAndCallback&) = delete;

    ~TokenAndCallback();

    int64_t token;
    KeyAvailableCb callback;
  };

  StarboardDrmKeyTracker();
  ~StarboardDrmKeyTracker();

  // Returns whether `key` exists. state_lock_ must be held when calling this
  // function.
  bool HasKeyLockHeld(const std::string& key);

  base::flat_map<std::string, base::flat_set<std::string>> session_id_to_keys_;

  // A lock for this class's state.
  base::Lock state_lock_;

  // Callbacks to be run once a given key is available.
  base::flat_map<std::string, std::vector<TokenAndCallback>> key_to_cb_;

  // The next token to be returned via WaitForKey.
  int64_t next_token_ = 0;

  // Note: any additional state added to this class should be cleared in
  // ClearStateForTesting.
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DRM_KEY_TRACKER_H_
