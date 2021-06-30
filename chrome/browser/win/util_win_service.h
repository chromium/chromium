// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_UTIL_WIN_SERVICE_H_
#define CHROME_BROWSER_WIN_UTIL_WIN_SERVICE_H_

#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// Spawns a new isolated instance of the Windows utility service and returns a
// remote interface to it. The lifetime of the service process is tied strictly
// to the lifetime of the returned Remote.
mojo::Remote<chrome::mojom::UtilWin> LaunchUtilWinServiceInstance();

#endif  // CHROME_BROWSER_WIN_UTIL_WIN_SERVICE_H_
