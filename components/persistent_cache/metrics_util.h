// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_METRICS_UTIL_H_
#define COMPONENTS_PERSISTENT_CACHE_METRICS_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

namespace persistent_cache {

enum class Client;

// Returns the name of a histogram of the form:
// "PersistentCache.metric{,.ReadWrite,.ReadOnly}.client".
std::string GetHistogramName(Client client,
                             std::string_view metric,
                             std::optional<bool> is_read_write = std::nullopt);

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_METRICS_UTIL_H_
