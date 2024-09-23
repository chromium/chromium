// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_TPM_METRICS_H_
#define CHROME_SERVICES_UTIL_WIN_TPM_METRICS_H_

#include "third_party/metrics_proto/system_profile.pb.h"

std::optional<metrics::SystemProfileProto_TpmIdentifier> GetTpmIdentifier(
    bool report_full_names);

#endif  // CHROME_SERVICES_UTIL_WIN_TPM_METRICS_H_
