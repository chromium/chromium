// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_SUGGESTIONS_STORE_H_
#define COMPONENTS_SUGGESTIONS_SUGGESTIONS_STORE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/clock.h"
#include "components/suggestions/proto/suggestions.pb.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace suggestions {

// A helper class for reading and writing the suggestions to the profile's
// preference file.
class SuggestionsStore {
 public:
  explicit SuggestionsStore(PrefService* profile_prefs);
  virtual ~SuggestionsStore();

  // Loads the suggestion data from the profile's preferences into
  // |suggestions|. If there is a problem with loading, the pref value is
  // cleared, false is returned and |suggestions| is cleared. If successful,
  // |suggestions| will contain the loaded data and true is returned.
  virtual bool LoadSuggestions(SuggestionsProfile* suggestions);

  // Stores the provided |suggestions| to the profile's preferences, using
  // a base64 encoding of its protobuf serialization.
  virtual bool StoreSuggestions(const SuggestionsProfile& suggestions);

  // Clears any suggestion data from the profile's preferences.
  virtual void ClearSuggestions();

  // Register SuggestionsStore related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void SetClockForTesting(base::Clock* clock);

 protected:
  // Test seam. For simplicity of mock creation.
  SuggestionsStore();

 private:
  // The pref service used to persist the suggestions data.
  PrefService* pref_service_;
  // Can be overridden for testing.
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsStore);

  // Filters expired suggestions.
  void FilterExpiredSuggestions(SuggestionsProfile* suggestions);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_SUGGESTIONS_STORE_H_
