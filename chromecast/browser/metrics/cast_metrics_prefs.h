// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_CAST_METRICS_PREFS_H_
#define CHROMECAST_BROWSER_METRICS_CAST_METRICS_PREFS_H_

class PrefRegistrySimple;

namespace chromecast {
namespace metrics {

void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_CAST_METRICS_PREFS_H_
