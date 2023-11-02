// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_H_

#include <stdint.h>
#include <iosfwd>
#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

namespace offline_pages {

// Metadata of the offline page.
struct OfflinePageItem {
 public:
  // Note that this should match with Flags enum in offline_pages.proto.
  enum Flags {
    NO_FLAG = 0,
    MARKED_FOR_DELETION = 0x1,
  };

  OfflinePageItem();
  OfflinePageItem(const GURL& url,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size);
  OfflinePageItem(const GURL& url,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size,
                  const base::Time& creation_time);
  OfflinePageItem(const OfflinePageItem& other);
  OfflinePageItem(OfflinePageItem&& other);

  ~OfflinePageItem();

  OfflinePageItem& operator=(const OfflinePageItem&);
  OfflinePageItem& operator=(OfflinePageItem&&);
  bool operator==(const OfflinePageItem& other) const;
  bool operator<(const OfflinePageItem& other) const;

  const GURL& GetOriginalUrl() const {
    return original_url_if_different.is_empty() ? url
                                                : original_url_if_different;
  }

  // The URL of the page. This is the last committed URL. In the case that
  // redirects occur, access |original_url| for the original URL.
  GURL url;
  // The primary key/ID for this page in offline pages internal database.
  int64_t offline_id = 0;

  // The Client ID (external) related to the offline page. This is opaque
  // to our system, but useful for users of offline pages who want to map
  // their ids to our saved pages.
  ClientId client_id;

  // The file path to the archive with a local copy of the page.
  base::FilePath file_path;
  // The size of the offline copy.
  int64_t file_size = 0;
  // The time when the offline archive was created.
  base::Time creation_time;
  // The time when the offline archive was last accessed.
  base::Time last_access_time;
  // Number of times that the offline archive has been accessed.
  int access_count = 0;
  // The title of the page at the time it was saved.
  std::u16string title;
  // Flags about the state and behavior of the offline page.
  Flags flags = NO_FLAG;
  // The original URL of the page if not empty. Otherwise, this is set to empty
  // and |url| should be accessed instead.
  GURL original_url_if_different;
  // The app, if any, that the item was saved on behalf of.
  // Empty string implies Chrome.
  std::string request_origin;
  // System download id.
  int64_t system_download_id = 0;
  // The most recent time when the file was discovered missing.
  // NULL time implies the file is not missing.
  base::Time file_missing_time;
  // Digest of the page calculated when page is saved, in order to tell if the
  // page can be trusted. This field will always be an empty string for
  // temporary and shared pages.
  std::string digest;
  // Snippet from the article.
  std::string snippet;
  // Text indicating the article's publisher.
  std::string attribution;
};

// This operator is for testing only, see offline_page_test_utils.cc.
// This is provided here to avoid ODR problems.
std::ostream& operator<<(std::ostream& out, const OfflinePageItem& value);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_H_
