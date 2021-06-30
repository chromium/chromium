// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PROCESSOR_METRICS_H_
#define CHROME_SERVICES_UTIL_WIN_PROCESSOR_METRICS_H_

// Records various metrics about the processor. This function will fail silently
// if these metrics can't be retrieved.
void RecordProcessorMetrics();

#endif  // CHROME_SERVICES_UTIL_WIN_PROCESSOR_METRICS_H_
