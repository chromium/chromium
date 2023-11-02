// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_PROCESS_UTILS_H_
#define CHROMECAST_BASE_PROCESS_UTILS_H_

#include <string>
#include <vector>

namespace chromecast {

// Executes application, for which arguments are specified by |argv| and wait
// for it to exit. Stores the output (stdout) in |output|. Returns true on
// success.
// TODO(slan): Replace uses of this with base::GetAppOutput when crbug/493711 is
// resolved.
bool GetAppOutput(const std::vector<std::string>& argv, std::string* output);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_PROCESS_UTILS_H_
