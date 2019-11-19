// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_H_

#include <ostream>

namespace ntp_snippets {

// These are the categories that the client knows about.
// The values before LOCAL_CATEGORIES_COUNT are the categories that are provided
// locally on the device. Categories provided by the server (IDs strictly larger
// than REMOTE_CATEGORIES_OFFSET) only need to be hard-coded here if they need
// to be recognized by the client implementation.
// NOTE: These are persisted, so don't reorder or remove values, and insert new
// values only in the appropriate places marked below.
// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class KnownCategories {
  // Pages recently downloaded during normal navigation.
  RECENT_TABS_DEPRECATED,

  // Pages downloaded by the user for offline consumption.
  DOWNLOADS_DEPRECATED,

  // Recently used bookmarks.
  BOOKMARKS_DEPRECATED,

  // Physical Web page available in the vicinity.
  PHYSICAL_WEB_PAGES_DEPRECATED,

  // Pages recently browsed to on other devices.
  FOREIGN_TABS_DEPRECATED,

  // Pages from the user reading list.
  READING_LIST,

  // ****************** INSERT NEW LOCAL CATEGORIES HERE! ******************
  // Existing categories are persisted and they must never be removed. This may
  // happen implicitly, e.g. when an older version without some local category
  // is installed.

  // Follows the last local category.
  LOCAL_CATEGORIES_COUNT,

  // Remote categories come after this.
  REMOTE_CATEGORIES_OFFSET = 10000,

  // Articles for you.
  ARTICLES = 10001,

  // Categories 10002-10008 are defined on the server.

  // ****************** INSERT NEW REMOTE CATEGORIES HERE! ******************
  // Update the list on the server first. Here specify the ID explicitly.

  // Tracks the last known remote category
  LAST_KNOWN_REMOTE_CATEGORY = ARTICLES,
};

// A category groups ContentSuggestions which belong together. Use the
// CategoryFactory to obtain instances.
class Category {
 public:
  // An arbitrary but consistent ordering. Can be used to look up categories in
  // a std::map, but should not be used to order categories for other purposes.
  struct CompareByID;

  // Creates a category from a KnownCategory value. The passed |known_category|
  // must not be one of the special values (LOCAL_CATEGORIES_COUNT or
  // REMOTE_CATEGORIES_OFFSET).
  static Category FromKnownCategory(KnownCategories known_category);

  // Creates a category from a category identifier delivered by the server.
  // |remote_category| must be positive.
  static Category FromRemoteCategory(int remote_category);

  // Creates a category from an ID as returned by |id()|. |id| must be a
  // non-negative value. Callers should make sure this is a valid id (if in
  // doubt, call IsValidIDValue()).
  static Category FromIDValue(int id);

  // Verifies if |id| is a valid ID value. Only checks that the value is within
  // a valid range -- not that the system actually knows about the corresponding
  // category.
  static bool IsValidIDValue(int id);

  // Returns a non-negative identifier that is unique for the category and can
  // be converted back to a Category instance using
  // |CategoryFactory::FromIDValue(id)|.
  int id() const { return id_; }

  // Returns a remote category identifier. Do not call for non-remote
  // categories.
  int remote_id() const;

  // Returns whether this category matches the given |known_category|.
  bool IsKnownCategory(KnownCategories known_category) const;

 private:
  explicit Category(int id);

  int id_;

  // Allow copy and assignment.
};

bool operator==(const Category& left, const Category& right);

bool operator!=(const Category& left, const Category& right);

struct Category::CompareByID {
  bool operator()(const Category& left, const Category& right) const;
};

std::ostream& operator<<(std::ostream& os, const Category& obj);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_H_
