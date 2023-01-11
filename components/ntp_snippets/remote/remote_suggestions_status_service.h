// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"

namespace ntp_snippets {

enum class RemoteSuggestionsStatus : int {
  // Suggestions are enabled and the user is signed in.
  ENABLED_AND_SIGNED_IN,
  // Suggestions are enabled; the user is signed out (sign-in is not required).
  ENABLED_AND_SIGNED_OUT,
  // Suggestions have been disabled as part of the service configuration.
  EXPLICITLY_DISABLED,
};

// Aggregates data from preferences and signin to notify the provider of
// relevant changes in their states.
class RemoteSuggestionsStatusService {
 public:
  using StatusChangeCallback =
      base::RepeatingCallback<void(RemoteSuggestionsStatus old_status,
                                   RemoteSuggestionsStatus new_status)>;
  virtual ~RemoteSuggestionsStatusService() = default;

  // Starts listening for changes from the dependencies. |callback| will be
  // called when a significant change in state is detected.
  virtual void Init(const StatusChangeCallback& callback) = 0;

  // To be called when the signin state changed. Will compute the new
  // state considering the initialisation configuration and the preferences,
  // and notify via the registered callback if appropriate.
  virtual void OnSignInStateChanged(bool has_signed_in) = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_H_
