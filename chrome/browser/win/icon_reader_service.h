// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_ICON_READER_SERVICE_H_
#define CHROME_BROWSER_WIN_ICON_READER_SERVICE_H_

#include "chrome/services/util_win/public/mojom/util_read_icon.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// Spawns a new isolated instance of the Windows icon utility service and
// returns a remote interface to it. The lifetime of the service process is tied
// strictly to the lifetime of the returned Remote.
mojo::Remote<chrome::mojom::UtilReadIcon> LaunchIconReaderInstance();

#endif  // CHROME_BROWSER_WIN_ICON_READER_SERVICE_H_
