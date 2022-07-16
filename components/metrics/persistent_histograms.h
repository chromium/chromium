// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PERSISTENT_HISTOGRAMS_H_
#define COMPONENTS_METRICS_PERSISTENT_HISTOGRAMS_H_

#include "base/files/file_path.h"

// Persistent browser metrics need to be persisted somewhere. This constant
// provides a known string to be used for both the allocator's internal name
// and for a file on disk (relative to metrics_dir) to which they
// can be saved. This is exported so the name can also be used as a "pref"
// during configuration.
extern const char kBrowserMetricsName[];

// Do all the checking and work necessary to enable persistent histograms.
void InstantiatePersistentHistograms(const base::FilePath& metrics_dir);

#endif  // COMPONENTS_METRICS_PERSISTENT_HISTOGRAMS_H_
