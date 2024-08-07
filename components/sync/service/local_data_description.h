// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
#define COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_

#include <string>
#include <vector>

#include "components/sync/base/data_type.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;

namespace syncer {

struct LocalDataDescription {
  // Actual count of local items.
  size_t item_count = 0;
  // Contains up to 3 distinct domains corresponding to some of the local items,
  // to be used for a preview.
  std::vector<std::string> domains;
  // Count of distinct domains for preview.
  // Note: This may be different from the count of items(`item_count`), since a
  // user might have, for e.g., multiple bookmarks or passwords for the same
  // domain. It may also be different from domains.size(), that one contains
  // only up to 3 elements.
  size_t domain_count = 0;

  LocalDataDescription();

  // `all_urls` should be the corresponding URL for each local data item, e.g.
  // the URL of each local bookmark. In the resulting object, fields will be as
  // below.
  //   item_count: The size of `all_urls`.
  //   domain_count: The number of unique domains in `all_urls`. For instance
  //                 for {a.com, a.com/foo and b.com}, domain_count will be 2.
  //   domains: The first (up to) 3 domains in alphabetical order.
  explicit LocalDataDescription(const std::vector<GURL>& all_urls);

  LocalDataDescription(const LocalDataDescription&);
  LocalDataDescription& operator=(const LocalDataDescription&);
  LocalDataDescription(LocalDataDescription&&);
  LocalDataDescription& operator=(LocalDataDescription&&);

  ~LocalDataDescription();

  friend bool operator==(const LocalDataDescription&,
                         const LocalDataDescription&) = default;
};

// Returns a string that summarizes the domain content of `description`, meant
// to be consumed by the UI. Must not be called if the `description.domains` is
// empty.
std::u16string GetDomainsDisplayText(const LocalDataDescription& description);

// gmock printer helper.
void PrintTo(const LocalDataDescription& local_data_description,
             std::ostream* os);

#if BUILDFLAG(IS_ANDROID)
// Constructs a Java LocalDataDescription from the provided C++
// LocalDataDescription
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaLocalDataDescription(
    JNIEnv* env,
    const syncer::LocalDataDescription& local_data_description);
#endif

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
