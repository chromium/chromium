// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CUSTOM_LINKS_STORE_H_
#define COMPONENTS_NTP_TILES_CUSTOM_LINKS_STORE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/ntp_tiles/custom_links_manager.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ntp_tiles {

// A helper class for reading and writing custom links to the profile's
// preference file. All virtual functions are for testing.
class CustomLinksStore {
 public:
  explicit CustomLinksStore(PrefService* prefs);

  CustomLinksStore(const CustomLinksStore&) = delete;
  CustomLinksStore& operator=(const CustomLinksStore&) = delete;

  // Virtual for testing.
  virtual ~CustomLinksStore();

  // Retrieves the custom link data from the profile's preferences and returns
  // them as a list of |Link|s. If there is a problem with retrieval, the pref
  // value is cleared and an empty list is returned.
  // Virtual for testing.
  virtual std::vector<CustomLinksManager::Link> RetrieveLinks();

  // Stores the provided |links| to the profile's preferences.
  // Virtual for testing.
  virtual void StoreLinks(const std::vector<CustomLinksManager::Link>& links);

  // Clears any custom link data from the profile's preferences.
  // Virtual for testing.
  virtual void ClearLinks();

  // Register CustomLinksStore related prefs in the Profile prefs.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

 private:
  // The pref service used to persist the custom link data.
  raw_ptr<PrefService> prefs_;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_CUSTOM_LINKS_STORE_H_
