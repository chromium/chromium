// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/browser_app_instance_update.h"

namespace apps {

BrowserAppInstanceUpdate::BrowserAppInstanceUpdate() = default;
BrowserAppInstanceUpdate::~BrowserAppInstanceUpdate() = default;
BrowserAppInstanceUpdate::BrowserAppInstanceUpdate(BrowserAppInstanceUpdate&&) =
    default;
BrowserAppInstanceUpdate& BrowserAppInstanceUpdate::operator=(
    BrowserAppInstanceUpdate&&) = default;

}  // namespace apps
