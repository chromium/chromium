// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_H_

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace previews {

using HintLoadedCallback =
    base::OnceCallback<void(const optimization_guide::proto::Hint&)>;

// Holds a set of optimization hints received from the Cacao service.
// This may include hints received from the ComponentUpdater and hints
// fetched from a Cacao Optimization Guide Service API. It will hold
// the host suffix keys of all cached hints (aka, activation list). It
// will manage a set of the associated hints details in memory and all
// of the hints in a persisted backing store.
class HintCache {
 public:
  HintCache();
  ~HintCache();

  // Returns whether the cache has a hint data for |host| locally (whether
  // in memory or persisted on disk).
  bool HasHint(const std::string& host) const;

  // Requests that hint data for |host| be loaded asynchronously and passed to
  // |callback| if/when loaded. |callback| will not be called if no hint data
  // is found for |host|.
  void LoadHint(const std::string& host, HintLoadedCallback callback);

  // Returns whether there is hint data available for |host| in memory.
  bool IsHintLoaded(const std::string& host) const;

  // Returns the hint data for |host| found in memory, otherwise nullptr.
  const optimization_guide::proto::Hint* GetHint(const std::string& host) const;

  // Adds |hint| to the cache.
  void AddHint(const optimization_guide::proto::Hint& hint);

  // Adds |hints| to the cache.
  void AddHints(const std::vector<optimization_guide::proto::Hint>& hints);

 private:
  // Returns the most specific host suffix of the host name that is present
  // in the activation list.
  std::string DetermineHostSuffix(const std::string& host) const;

  // The set of host suffixes that have Hint data.
  std::unordered_set<std::string> activation_list_;

  // The in-memory cache of hints. Maps host suffix to Hint proto.
  // TODO(dougarnett): Add MRU subset support (mru_cache) with backing store.
  std::map<std::string, optimization_guide::proto::Hint> memory_cache_;

  // TODO(dougarnett): Add a backing store with all hints.

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintCache);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_H_
