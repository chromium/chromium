// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_TESTUTILS_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_TESTUTILS_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/update_service.h"

namespace updater {

base::RepeatingCallback<scoped_refptr<UpdateService>()> MakeFakeService(
    UpdateService::Result result,
    const std::vector<UpdateService::AppState>& states);

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_TESTUTILS_H_
