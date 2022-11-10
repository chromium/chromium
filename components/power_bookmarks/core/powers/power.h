// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWERS_POWER_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWERS_POWER_H_

#include "base/guid.h"
#include "base/time/time.h"
#include "components/power_bookmarks/core/proto/power_bookmark_specifics.pb.h"
#include "url/gurl.h"

namespace power_bookmarks {

class PowerSpecifics;

// Class for the in-memory representation for Powers.
// When writing to local storage or sync, this class is written to the
// save_specifics proto.
class Power {
 public:
  // ctor used for creating a Power in-memory.
  explicit Power(std::unique_ptr<PowerSpecifics> power_specifics);
  // ctor used for creating a Power from the db.
  explicit Power(const PowerBookmarkSpecifics& specifics);

  Power(const Power&) = delete;
  Power& operator=(const Power&) = delete;

  ~Power();

  const base::GUID& guid() const { return guid_; }
  void set_guid(base::GUID guid) { guid_ = guid; }

  const GURL& url() const { return url_; }
  void set_url(GURL url) { url_ = url; }

  const PowerType& power_type() const { return power_type_; }
  void set_power_type(PowerType power_type) { power_type_ = power_type; }

  const base::Time time_added() const { return time_added_; }
  void set_time_added(base::Time time_added) { time_added_ = time_added; }

  const base::Time time_modified() const { return time_modified_; }
  void set_time_modified(base::Time time_modified) {
    time_modified_ = time_modified;
  }

  // Write the properties held in this class to power_bookmark_specifics.proto.
  // `power_bookmark_specifics` will never be nullptr.
  void ToPowerBookmarkSpecifics(PowerBookmarkSpecifics* save_specifics);

 private:
  base::GUID guid_;
  GURL url_;
  PowerType power_type_;
  base::Time time_modified_;
  base::Time time_added_;
  std::unique_ptr<PowerSpecifics> power_specifics_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWERS_POWER_H_