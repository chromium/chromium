// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_UTIL_WIN_SERVICE_H_
#define CHROME_BROWSER_WIN_UTIL_WIN_SERVICE_H_

#include "chrome/services/util_win/public/mojom/util_win.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// Spawns a new isolated instance of the Windows utility service and returns a
// remote interface to it. The lifetime of the service process is tied strictly
// to the lifetime of the returned Remote.
mojo::Remote<chrome::mojom::UtilWin> LaunchUtilWinServiceInstance();

// Spawns a new isolated instance of the Windows processor metrics service and
// returns a remote interface to it. The lifetime of the service process is tied
// strictly to the lifetime of the returned Remote.
mojo::Remote<chrome::mojom::ProcessorMetrics> LaunchProcessorMetricsService();

#endif  // CHROME_BROWSER_WIN_UTIL_WIN_SERVICE_H_
