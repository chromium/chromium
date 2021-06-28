// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_BLOCKLIST_STORE_H_
#define COMPONENTS_SUGGESTIONS_BLOCKLIST_STORE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "url/gurl.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace suggestions {

// A helper class for reading, writing, modifying and applying a small URL
// blocklist, pending upload to the server. The class has a concept of time
// duration before which a blocklisted URL becomes candidate for upload to the
// server. Keep in mind most of the operations involve interaction with the disk
// (the profile's preferences). Note that the class should be used as a
// singleton for the upload candidacy to work properly.
class BlocklistStore {
 public:
  explicit BlocklistStore(
      PrefService* profile_prefs,
      const base::TimeDelta& upload_delay = base::TimeDelta::FromSeconds(15));
  virtual ~BlocklistStore();

  // Returns true if successful or |url| was already in the blocklist. If |url|
  // was already in the blocklist, its blocklisting timestamp gets updated.
  virtual bool BlocklistUrl(const GURL& url);

  // Clears any blocklist data from the profile's preferences.
  virtual void ClearBlocklist();

  // Gets the time until any URL is ready for upload. Returns false if the
  // blocklist is empty.
  virtual bool GetTimeUntilReadyForUpload(base::TimeDelta* delta);

  // Gets the time until |url| is ready for upload. Returns false if |url| is
  // not part of the blocklist.
  virtual bool GetTimeUntilURLReadyForUpload(const GURL& url,
                                             base::TimeDelta* delta);

  // Sets |url| to a URL from the blocklist that is candidate for upload.
  // Returns false if there is no candidate for upload.
  virtual bool GetCandidateForUpload(GURL* url);

  // Removes |url| from the stored blocklist. Returns true if successful, false
  // on failure or if |url| was not in the blocklist. Note that this function
  // does not enforce a minimum time since blocklist before removal.
  virtual bool RemoveUrl(const GURL& url);

  // Applies the blocklist to |suggestions|.
  virtual void FilterSuggestions(SuggestionsProfile* suggestions);

  // Register BlocklistStore related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  // Test seam. For simplicity of mock creation.
  BlocklistStore();

  // Loads the blocklist data from the Profile preferences into
  // |blocklist|. If there is a problem with loading, the pref value is
  // cleared, false is returned and |blocklist| is cleared. If successful,
  // |blocklist| will contain the loaded data and true is returned.
  bool LoadBlocklist(SuggestionsBlocklist* blocklist);

  // Stores the provided |blocklist| to the Profile preferences, using
  // a base64 encoding of its protobuf serialization.
  bool StoreBlocklist(const SuggestionsBlocklist& blocklist);

 private:
  // The pref service used to persist the suggestions blocklist.
  PrefService* pref_service_;

  // Delay after which a URL becomes candidate for upload, measured from the
  // last time the URL was added.
  base::TimeDelta upload_delay_;

  // The times at which URLs were blocklisted. Used to determine when a URL is
  // valid for server upload. Guaranteed to contain URLs that are not ready for
  // upload. Might not contain URLs that are ready for upload.
  std::map<std::string, base::TimeTicks> blocklist_times_;

  DISALLOW_COPY_AND_ASSIGN(BlocklistStore);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_BLOCKLIST_STORE_H_
