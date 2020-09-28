// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_STORE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"

namespace video_tutorials {

// Interface of video tutorials collection store.
template <typename T>
class Store {
 public:
  using Keys = std::unique_ptr<std::vector<std::string>>;
  using Entries = std::vector<std::unique_ptr<T>>;
  using LoadKeysCallback = base::OnceCallback<void(bool, Keys)>;
  using LoadEntriesCallback = base::OnceCallback<void(bool, Entries)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;

  // Initialize the db and load keys into memory.
  virtual void InitAndLoadKeys(LoadKeysCallback callback) = 0;

  // Load entries with the given keys into memory.
  virtual void LoadEntries(const std::vector<std::string>& keys,
                           LoadEntriesCallback callback) = 0;

  // Batch update call with ability to update and remove multiple
  // entries.
  virtual void UpdateAll(
      const std::vector<std::pair<std::string, T>>& key_entry_pairs,
      const std::vector<std::string>& keys_to_delete,
      UpdateCallback callback) = 0;

  Store() = default;
  virtual ~Store() = default;

  Store(const Store& other) = delete;
  Store& operator=(const Store& other) = delete;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_STORE_H_
