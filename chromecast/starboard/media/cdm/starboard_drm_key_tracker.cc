// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"

#include <algorithm>

#include "starboard_drm_key_tracker.h"

namespace chromecast {
namespace media {

StarboardDrmKeyTracker::StarboardDrmKeyTracker() = default;
StarboardDrmKeyTracker::~StarboardDrmKeyTracker() = default;

StarboardDrmKeyTracker& StarboardDrmKeyTracker::GetInstance() {
  static base::NoDestructor<StarboardDrmKeyTracker> instance;
  return *instance;
}

void StarboardDrmKeyTracker::AddKey(const std::string& key,
                                    const std::string& session_id) {
  std::vector<TokenAndCallback> tokens_and_callbacks;
  {
    base::AutoLock lock(state_lock_);
    session_id_to_keys_[session_id].insert(key);
    auto it = key_to_cb_.find(key);
    if (it == key_to_cb_.end()) {
      // No callbacks to run.
      return;
    }
    tokens_and_callbacks = std::move(it->second);
    key_to_cb_.erase(it);
  }  // state_lock_ is released.

  // Run any callbacks that were waiting for key to be available.
  for (auto& token_and_cb : tokens_and_callbacks) {
    const int64_t token = token_and_cb.token;
    std::move(token_and_cb.callback).Run(token);
  }
}

void StarboardDrmKeyTracker::RemoveKey(const std::string& key,
                                       const std::string& session_id) {
  base::AutoLock lock(state_lock_);
  auto it = session_id_to_keys_.find(session_id);
  if (it == session_id_to_keys_.end()) {
    return;
  }
  it->second.erase(key);
}

bool StarboardDrmKeyTracker::HasKey(const std::string& key) {
  base::AutoLock lock(state_lock_);
  return HasKeyLockHeld(key);
}

void StarboardDrmKeyTracker::RemoveKeysForSession(
    const std::string& session_id) {
  base::AutoLock lock(state_lock_);
  session_id_to_keys_.erase(session_id);
}

int64_t StarboardDrmKeyTracker::WaitForKey(const std::string& key,
                                           KeyAvailableCb cb) {
  // Avoid holding the lock when running `cb` in the case where key is already
  // available.
  int64_t token = 0;
  {
    base::AutoLock lock(state_lock_);
    token = next_token_++;
    if (!HasKeyLockHeld(key)) {
      key_to_cb_[key].push_back(TokenAndCallback(token, std::move(cb)));
      return token;
    }
  }  // state_lock_ is released.

  // We already have `key`, so run the callback immediately.
  std::move(cb).Run(token);
  return token;
}

void StarboardDrmKeyTracker::UnregisterCallback(int64_t callback_token) {
  base::AutoLock lock(state_lock_);
  for (auto& key_and_vec : key_to_cb_) {
    std::vector<TokenAndCallback>& tokens_and_cbs = key_and_vec.second;
    for (size_t i = 0; i < tokens_and_cbs.size(); ++i) {
      if (tokens_and_cbs[i].token == callback_token) {
        // Remove the callback and token. For a vector this can be done in O(1)
        // by swapping with the last element, since we do not care about the
        // order of the elements in the vector.
        std::swap(tokens_and_cbs[i], tokens_and_cbs.back());
        tokens_and_cbs.pop_back();
        return;
      }
    }
  }
}

void StarboardDrmKeyTracker::ClearStateForTesting() {
  base::AutoLock lock(state_lock_);
  session_id_to_keys_.clear();
  key_to_cb_.clear();
  next_token_ = 0;
}

bool StarboardDrmKeyTracker::HasKeyLockHeld(const std::string& key) {
  state_lock_.AssertAcquired();
  return std::any_of(session_id_to_keys_.cbegin(), session_id_to_keys_.cend(),
                     [&](const auto& session_id_and_keys) {
                       return session_id_and_keys.second.contains(key);
                     });
}

StarboardDrmKeyTracker::TokenAndCallback::TokenAndCallback(
    int64_t token,
    KeyAvailableCb callback) {
  this->token = token;
  this->callback = std::move(callback);
}

StarboardDrmKeyTracker::TokenAndCallback::~TokenAndCallback() = default;
StarboardDrmKeyTracker::TokenAndCallback::TokenAndCallback(TokenAndCallback&&) =
    default;
StarboardDrmKeyTracker::TokenAndCallback&
StarboardDrmKeyTracker::TokenAndCallback::operator=(TokenAndCallback&&) =
    default;

}  // namespace media
}  // namespace chromecast
