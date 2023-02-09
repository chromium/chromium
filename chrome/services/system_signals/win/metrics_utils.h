// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_WIN_METRICS_UTILS_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_WIN_METRICS_UTILS_H_

#include "components/device_signals/core/system_signals/win/wmi_client.h"
#include "components/device_signals/core/system_signals/win/wsc_client.h"

namespace system_signals {

// Logs UMA metrics related to the number of items and errors contained in
// `response`.
void LogWscAvResponse(const device_signals::WscAvProductsResponse& response);

// Logs UMA metrics related to the number of items and errors contained in
// `response`.
void LogWmiHotfixResponse(const device_signals::WmiHotfixesResponse& response);

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_WIN_METRICS_UTILS_H_
