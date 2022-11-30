// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_MAC_SERVER_H_
#define CHROME_UPDATER_APP_SERVER_MAC_SERVER_H_

#include "base/memory/scoped_refptr.h"

namespace updater {

class App;

scoped_refptr<App> MakeAppServer();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_MAC_SERVER_H_
