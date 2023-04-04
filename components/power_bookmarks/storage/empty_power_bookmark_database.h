// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_EMPTY_POWER_BOOKMARK_DATABASE_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_EMPTY_POWER_BOOKMARK_DATABASE_H_

#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_overview.h"
#include "components/power_bookmarks/storage/power_bookmark_database.h"
#include "url/gurl.h"

namespace power_bookmarks {

struct SearchParams;

// Fake database to substitute when the feature is disabled.
class EmptyPowerBookmarkDatabase : public PowerBookmarkDatabase {
 public:
  EmptyPowerBookmarkDatabase();
  EmptyPowerBookmarkDatabase(const EmptyPowerBookmarkDatabase&) = delete;
  EmptyPowerBookmarkDatabase& operator=(const EmptyPowerBookmarkDatabase&) =
      delete;

  ~EmptyPowerBookmarkDatabase() override;

  // PowerBookmarkDatabase implementation
  bool Init() override;
  bool IsOpen() override;
  std::vector<std::unique_ptr<Power>> GetPowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) override;
  std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForType(
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) override;
  std::vector<std::unique_ptr<Power>> GetPowersForSearchParams(
      const SearchParams& search_params) override;
  std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForSearchParams(
      const SearchParams& search_params) override;
  bool CreatePower(std::unique_ptr<Power> power) override;
  std::unique_ptr<Power> UpdatePower(std::unique_ptr<Power> power) override;
  bool DeletePower(const base::Uuid& guid) override;
  bool DeletePowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
      std::vector<std::string>* deleted_guids) override;
  std::vector<std::unique_ptr<Power>> GetAllPowers() override;
  std::vector<std::unique_ptr<Power>> GetPowersForGUIDs(
      const std::vector<std::string>& guids) override;
  std::unique_ptr<Power> GetPowerForGUID(const std::string& guid) override;
  bool CreateOrMergePowerFromSync(const Power& power) override;
  bool DeletePowerFromSync(const std::string& guid) override;
  PowerBookmarkSyncMetadataDatabase* GetSyncMetadataDatabase() override;
  std::unique_ptr<Transaction> BeginTransaction() override;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_EMPTY_POWER_BOOKMARK_DATABASE_H_
