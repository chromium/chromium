// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_SHELF_ID_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_SHELF_ID_H_

#include <string>

namespace arc {

// An ARC app shelf id consisting of an app id (must be a valid crx file id)
// and an optional shelf group id. Ids with empty app ids are invalid.
class ArcAppShelfId {
 public:
  ArcAppShelfId();
  ArcAppShelfId(const std::string& shelf_group_id, const std::string& app_id);
  ArcAppShelfId(const ArcAppShelfId& other);
  ~ArcAppShelfId();

  // Returns an id from a string with syntax "shelf_group:group_id:app_id".
  // If the shelf_group prefix is absent then the input is treated as an app id.
  // In either case, if the app_id is not a valid crx file id, then the returned
  // ArcAppShelfId is empty and considered invalid.
  static ArcAppShelfId FromString(const std::string& id);

  // Constructs id from app id and optional shelf_group_id encoded into the
  // |intent|.
  static ArcAppShelfId FromIntentAndAppId(const std::string& intent,
                                          const std::string& app_id);

  // Returns string representation of this id.
  std::string ToString() const;

  bool operator<(const ArcAppShelfId& other) const;

  bool has_shelf_group_id() const { return !shelf_group_id_.empty(); }

  const std::string& shelf_group_id() const { return shelf_group_id_; }

  const std::string& app_id() const { return app_id_; }

  bool valid() const { return !app_id_.empty(); }

 private:
  const std::string shelf_group_id_;
  const std::string app_id_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_SHELF_ID_H_
