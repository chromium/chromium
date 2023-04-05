// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_H_
#define COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_H_

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "url/gurl.h"

namespace power_bookmarks {

// Class for the in-memory representation for Powers.
// When writing to local storage or sync, this class is written to the
// save_specifics proto.
class Power {
 public:
  // ctor used for creating a Power in-memory.
  explicit Power(std::unique_ptr<sync_pb::PowerEntity> power_entity);
  // ctor used for creating a Power from the db.
  explicit Power(const sync_pb::PowerBookmarkSpecifics& specifics);

  Power(const Power&) = delete;
  Power& operator=(const Power&) = delete;

  ~Power();

  const base::Uuid& guid() const { return guid_; }
  void set_guid(base::Uuid guid) { guid_ = guid; }

  std::string guid_string() const { return guid_.AsLowercaseString(); }

  const GURL& url() const { return url_; }
  void set_url(GURL url) { url_ = url; }

  const sync_pb::PowerBookmarkSpecifics::PowerType& power_type() const {
    return power_type_;
  }
  void set_power_type(sync_pb::PowerBookmarkSpecifics::PowerType power_type) {
    power_type_ = power_type;
  }

  const base::Time time_added() const { return time_added_; }
  void set_time_added(base::Time time_added) { time_added_ = time_added; }

  const base::Time time_modified() const { return time_modified_; }
  void set_time_modified(base::Time time_modified) {
    time_modified_ = time_modified;
  }

  // Used to get fields from PowerEntity.
  const sync_pb::PowerEntity* power_entity() const {
    return power_entity_.get();
  }

  // Used to set fields to PowerEntity.
  sync_pb::PowerEntity* power_entity() { return power_entity_.get(); }

  // Write the properties held in this class to power_bookmark_specifics.proto.
  // `power_bookmark_specifics` will never be nullptr.
  void ToPowerBookmarkSpecifics(
      sync_pb::PowerBookmarkSpecifics* save_specifics) const;

  // Merge the curr sync_pb::PowerBookmarkSpecifics::PowerType other one.
  // guid, url and sync_pb::PowerBookmarkSpecifics::PowerType are not allowed to
  // be different from the other power.
  void Merge(const Power& other);

  // Clone a power. Every power should be independent to others.
  // This should be call to duplicate a power instead of direct assign.
  std::unique_ptr<Power> Clone() const;

 private:
  base::Uuid guid_;
  GURL url_;
  sync_pb::PowerBookmarkSpecifics::PowerType power_type_;
  base::Time time_modified_;
  base::Time time_added_;
  std::unique_ptr<sync_pb::PowerEntity> power_entity_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_H_
