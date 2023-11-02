// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_IMPL_INACTIVE_H_
#define CHROME_UPDATER_UPDATE_SERVICE_IMPL_INACTIVE_H_

#include "base/memory/scoped_refptr.h"

namespace updater {

class UpdateService;

scoped_refptr<UpdateService> MakeInactiveUpdateService();

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_IMPL_INACTIVE_H_
