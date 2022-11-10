// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/empty_power_bookmark_database.h"

#include "components/power_bookmarks/core/proto/power_bookmark_specifics.pb.h"
#include "url/origin.h"

namespace power_bookmarks {

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
    const PowerType& power_type) {
  return std::vector<std::unique_ptr<Power>>();
}

std::vector<std::unique_ptr<PowerOverview>>
EmptyPowerBookmarkDatabase::GetPowerOverviewsForType(
    const PowerType& power_type) {
  return std::vector<std::unique_ptr<PowerOverview>>();
}

bool EmptyPowerBookmarkDatabase::CreatePower(std::unique_ptr<Power> power) {
  return false;
}

bool EmptyPowerBookmarkDatabase::UpdatePower(std::unique_ptr<Power> power) {
  return false;
}

bool EmptyPowerBookmarkDatabase::DeletePower(const base::GUID& guid) {
  return false;
}

bool EmptyPowerBookmarkDatabase::DeletePowersForURL(
    const GURL& url,
    const PowerType& power_type) {
  return false;
}

}  // namespace power_bookmarks
