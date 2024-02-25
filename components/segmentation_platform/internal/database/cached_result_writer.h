// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CACHED_RESULT_WRITER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CACHED_RESULT_WRITER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/client_results.pb.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"

namespace segmentation_platform {
struct Config;
class ClientResultPrefs;

// CachedResultWriter layer writes client results to prefs if the result in
// prefs for the given client has expired or not present. It also get the fresh
// results for the  client after model execution inorder to update it in prefs.
class CachedResultWriter {
 public:
  CachedResultWriter(ClientResultPrefs* prefs, base::Clock* clock);

  ~CachedResultWriter();

  // Disallow copy/assign.
  CachedResultWriter(CachedResultWriter&) = delete;
  CachedResultWriter& operator=(CachedResultWriter&) = delete;

  // Updates the prefs only if the previous result in the pref is expired or
  // unavailable or `force_refresh_results` is set as true. Returns true if
  // prefs was updated.
  bool UpdatePrefsIfExpired(const Config* config,
                            proto::ClientResult client_result,
                            const PlatformOptions& platform_options);

  // Marks the result as used by client. Does not change the result. Should be
  // called when the client code uses the result cached in prefs. Only saves the
  // first time the result is used. This is valid till the result is deleted and
  // new result is written.
  void MarkResultAsUsed(const Config* config);

  // Writes model execution results to prefs. This would overwrite the results
  // stored in prefs without verifying the TTL of existing results. Can be used
  // when the client is using the new `result` already and pref is stale even
  // before TTL expiry.
  void CacheModelExecution(const Config* config,
                           const proto::PredictionResult& result);

 private:
  // Checks the following to determine whether to update pref with new result.
  // 1. Previous model results for client are either expired or unavailable.
  // 2. `force_refresh_results` option is set to true.
  bool IsPrefUpdateRequiredForClient(const Config* config,
                                     const proto::ClientResult& client_result,
                                     const PlatformOptions& platform_options);

  // Updates the supplied `client_result` as new result for the client in prefs.
  void UpdateNewClientResultToPrefs(const Config* config,
                                    proto::ClientResult client_result);

  // Helper class to read/write results to the prefs.
  const raw_ptr<ClientResultPrefs> result_prefs_;

  // The time provider.
  const raw_ptr<base::Clock> clock_;

  base::WeakPtrFactory<CachedResultWriter> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CACHED_RESULT_WRITER_H_
