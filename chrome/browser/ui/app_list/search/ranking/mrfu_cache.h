// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_MRFU_CACHE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_MRFU_CACHE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/ranking/mrfu_cache.pb.h"
#include "chrome/browser/ui/app_list/search/ranking/persistent_proto.h"

namespace app_list {

// The most-recently-frequently-used cache stores a mapping from strings -
// called items - to scores that is persisted to disk. An item's score is
// increased when |Use| is called, and decays over time as |Use| is called on
// other items.
//
// TODO(crbug.com/1199206): Expand on details once implemented.
class MrfuCache {
 public:
  explicit MrfuCache(const base::FilePath& path);
  ~MrfuCache();

  MrfuCache(const MrfuCache&) = delete;
  MrfuCache& operator=(const MrfuCache&) = delete;

  void Use(const std::string& value);

  float Get(const std::string& value);

 private:
  PersistentProto<MrfuCacheProto> proto_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_MRFU_CACHE_H_
