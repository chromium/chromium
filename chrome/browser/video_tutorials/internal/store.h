// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_STORE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace video_tutorials {

// Interface of video tutorials metadata store.
template <typename T>
class Store {
 public:
  using Entries = std::unique_ptr<std::vector<T>>;
  using LoadCallback = base::OnceCallback<void(bool, std::unique_ptr<T>)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;

  // Initializes and loads the one and only database entry into memory. If
  // invoked for the second time, it will skip initialization and proceeds with
  // the load step.
  virtual void InitAndLoad(LoadCallback callback) = 0;

  // Updates the one and only database entry.
  virtual void Update(const T& entry, UpdateCallback callback) = 0;

  Store() = default;
  virtual ~Store() = default;

  Store(const Store& other) = delete;
  Store& operator=(const Store& other) = delete;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_STORE_H_
