// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/empty_power_bookmark_database.h"

#include "components/power_bookmarks/common/search_params.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "url/gurl.h"

namespace power_bookmarks {

namespace {
class EmptyDatabaseTransaction : public Transaction {
 public:
  bool Commit() override;
};

bool EmptyDatabaseTransaction::Commit() {
  return true;
}
}  // namespace

EmptyPowerBookmarkDatabase::EmptyPowerBookmarkDatabase() = default;

EmptyPowerBookmarkDatabase::~EmptyPowerBookmarkDatabase() = default;

bool EmptyPowerBookmarkDatabase::Init() {
  return true;
}

bool EmptyPowerBookmarkDatabase::IsOpen() {
  return true;
}

std::vector<std::unique_ptr<Power>> EmptyPowerBookmarkDatabase::GetPowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  return std::vector<std::unique_ptr<Power>>();
}

std::vector<std::unique_ptr<PowerOverview>>
EmptyPowerBookmarkDatabase::GetPowerOverviewsForType(
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  return std::vector<std::unique_ptr<PowerOverview>>();
}

std::vector<std::unique_ptr<Power>>
EmptyPowerBookmarkDatabase::GetPowersForSearchParams(
    const SearchParams& search_params) {
  return std::vector<std::unique_ptr<Power>>();
}

std::vector<std::unique_ptr<PowerOverview>>
EmptyPowerBookmarkDatabase::GetPowerOverviewsForSearchParams(
    const SearchParams& search_params) {
  return std::vector<std::unique_ptr<PowerOverview>>();
}

bool EmptyPowerBookmarkDatabase::CreatePower(std::unique_ptr<Power> power) {
  return false;
}

std::unique_ptr<Power> EmptyPowerBookmarkDatabase::UpdatePower(
    std::unique_ptr<Power> power) {
  return nullptr;
}

bool EmptyPowerBookmarkDatabase::DeletePower(const base::Uuid& guid) {
  return false;
}

bool EmptyPowerBookmarkDatabase::DeletePowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    std::vector<std::string>* deleted_guids) {
  return false;
}

std::vector<std::unique_ptr<Power>> EmptyPowerBookmarkDatabase::GetAllPowers() {
  return std::vector<std::unique_ptr<Power>>();
}

std::vector<std::unique_ptr<Power>>
EmptyPowerBookmarkDatabase::GetPowersForGUIDs(
    const std::vector<std::string>& guids) {
  return std::vector<std::unique_ptr<Power>>();
}

std::unique_ptr<Power> EmptyPowerBookmarkDatabase::GetPowerForGUID(
    const std::string& guid) {
  return nullptr;
}

bool EmptyPowerBookmarkDatabase::CreateOrMergePowerFromSync(
    const Power& power) {
  return false;
}

bool EmptyPowerBookmarkDatabase::DeletePowerFromSync(const std::string& guid) {
  return false;
}

PowerBookmarkSyncMetadataDatabase*
EmptyPowerBookmarkDatabase::GetSyncMetadataDatabase() {
  return nullptr;
}

std::unique_ptr<Transaction> EmptyPowerBookmarkDatabase::BeginTransaction() {
  return std::make_unique<EmptyDatabaseTransaction>();
}

}  // namespace power_bookmarks
