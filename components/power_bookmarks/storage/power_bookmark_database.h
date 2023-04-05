// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_H_

#include <memory>
#include <vector>

#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_overview.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_bridge.h"
#include "url/gurl.h"

namespace power_bookmarks {

struct SearchParams;

// Interface for the database layer of the Power Bookmark database.
class PowerBookmarkDatabase : public PowerBookmarkSyncBridge::Delegate {
 public:
  virtual ~PowerBookmarkDatabase() = default;

  // Initialises internal database. Must be called prior to any other usage.
  virtual bool Init() = 0;

  // Returns whether the database is currently open.
  virtual bool IsOpen() = 0;

  // Returns a vector of Powers for the given `url`. Use `power_type` to
  // restrict which type is returned or use POWER_TYPE_UNSPECIFIED to return
  // everything.
  virtual std::vector<std::unique_ptr<Power>> GetPowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) = 0;

  // Returns a vector of PowerOverviews for the given `power_type`.
  virtual std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForType(
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) = 0;

  // Returns a vector of Powers for the given `search_params`.
  virtual std::vector<std::unique_ptr<Power>> GetPowersForSearchParams(
      const SearchParams& search_params) = 0;

  // Returns a vector of PowerOverviews for the given `search_params`.
  virtual std::vector<std::unique_ptr<PowerOverview>>
  GetPowerOverviewsForSearchParams(const SearchParams& search_params) = 0;

  // Create the given `power` in the database. If it already exists, then it
  // will be updated. Returns whether the operation was successful.
  virtual bool CreatePower(std::unique_ptr<Power> power) = 0;

  // Update the given `power` in the database. If it doesn't exist, then it
  // will be created instead. Returns the updated power if the operation was
  // successful or nullptr otherwise.
  virtual std::unique_ptr<Power> UpdatePower(std::unique_ptr<Power> power) = 0;

  // Delete the given `guid` in the database, if it exists. Returns whether
  // the operation was successful.
  virtual bool DeletePower(const base::Uuid& guid) = 0;

  // Delete all powers for the given `url`. Use `power_type` to restrict which
  // type is deleted or use POWER_TYPE_UNSPECIFIED to delete everything.
  // Returns whether the operation was successfaul and all deleted guids in
  // deleted_guids as output if provided.
  virtual bool DeletePowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
      std::vector<std::string>* deleted_guids = nullptr) = 0;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_H_
