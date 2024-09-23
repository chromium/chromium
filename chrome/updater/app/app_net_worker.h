// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_NET_WORKER_H_
#define CHROME_UPDATER_APP_APP_NET_WORKER_H_

#include "base/memory/scoped_refptr.h"

namespace updater {

class App;

scoped_refptr<App> MakeAppNetWorker();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_NET_WORKER_H_
