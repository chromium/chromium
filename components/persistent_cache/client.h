// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_CLIENT_H_
#define COMPONENTS_PERSISTENT_CACHE_CLIENT_H_

namespace persistent_cache {

// An identifier of the client of a PersistentCache that is used for metrics
// reporting.
// LINT.IfChange(Client)
enum class Client {
  kCodeCache,
  kShaderCache,
  kTest,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/persistent_cache/histograms.xml:Client)

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_CLIENT_H_
