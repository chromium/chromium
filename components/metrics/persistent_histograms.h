// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PERSISTENT_HISTOGRAMS_H_
#define COMPONENTS_METRICS_PERSISTENT_HISTOGRAMS_H_

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"

// Feature definition for enabling histogram persistence. Note that this feature
// (along with its param `kPersistentHistogramsStorage`, declared below) is not
// used for Chrome on Linux, ChromeOS, Windows, macOS, and Android. Instead,
// histograms are persisted to a memory-mapped file, and set up before field
// trial initialization (see //chrome/app/chrome_main_delegate.cc).
BASE_DECLARE_FEATURE(kPersistentHistogramsFeature);

// If `kPersistentHistogramsStorage` is set to this, histograms will be
// allocated in a memory region backed by a file.
extern const char kPersistentHistogramStorageMappedFile[];

// If `kPersistentHistogramsStorage` is set to this, histograms will be
// allocated on the heap, but using the same allocator as the one used for
// file-backed persistent histograms.
extern const char kPersistentHistogramStorageLocalMemory[];

// Determines where histograms will be allocated (should either be
// `kPersistentHistogramStorageMappedFile` or
// `kPersistentHistogramStorageLocalMemory`).
extern const base::FeatureParam<std::string> kPersistentHistogramsStorage;

// Persistent browser metrics need to be persisted somewhere. This constant
// provides a known string to be used for both the allocator's internal name and
// for a file on disk (relative to metrics_dir) to which they can be saved. This
// is exported so the name can also be used as a "pref" during configuration.
extern const char kBrowserMetricsName[];

// Like above, this provides a known string to be used for persistent browser
// metrics. However, metrics under this are "deferred" and sent along with a
// future session's metrics instead of independently.
extern const char kDeferredBrowserMetricsName[];

// Do all the checking and work necessary to enable persistent histograms.
// `metrics_dir` specifies the root directory where persistent histograms will
// live. If `persistent_histograms_enabled` is false, this is essentially a
// no-op (histograms will continue being allocated on the heap). Otherwise,
// `storage`, which should be either `kPersistentHistogramStorageMappedFile` or
// `kPersistentHistogramStorageLocalMemory`, determines where histograms will be
// allocated.
// Note: After a call to this, a call toPersistentHistogramsCleanup() below
// should be made when appropriate.
void InstantiatePersistentHistograms(const base::FilePath& metrics_dir,
                                     bool persistent_histograms_enabled,
                                     std::string_view storage);

// Schedule the tasks required to cleanup the persistent metrics files.
void PersistentHistogramsCleanup(const base::FilePath& metrics_dir);

// Calls InstantiatePersistentHistograms() using `kPersistentHistogramsFeature`
// and `kPersistentHistogramsStorage` as params. PersistentHistogramsCleanup()
// is also called immediately after.
void InstantiatePersistentHistogramsWithFeaturesAndCleanup(
    const base::FilePath& metrics_dir);

// After calling this, histograms from this session that were not sent (i.e.,
// unlogged samples) will be sent along with a future session's metrics.
// Normally, those unlogged samples are sent as an "independent log", which uses
// the system profile in the persistent file. However, there are scenarios where
// the browser must exit before a system profile is written to the file, which
// results in the metrics from that session being lost. Calling this function
// will make so that the unlogged samples are sent with a future session's
// metrics (whichever is the first to read the file). Note that this may come at
// the cost of associating the metrics with an incorrect system profile. Returns
// whether the operation was successful.
// Note: If you plan on using this, please make sure to get a review from the
// metrics team. Using this may lead to reporting additional histograms that
// may not be relevant to what is being experimented with, which can cause
// confusing/disruptive data.
bool DeferBrowserMetrics(const base::FilePath& metrics_dir);

#endif  // COMPONENTS_METRICS_PERSISTENT_HISTOGRAMS_H_
