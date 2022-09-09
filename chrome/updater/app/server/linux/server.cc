// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/linux/server.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/updater/app/app.h"

namespace updater {

// TODO(crbug.com/1276117) - implement.
scoped_refptr<App> MakeAppServer() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace updater
