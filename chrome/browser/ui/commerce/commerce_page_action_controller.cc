// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/commerce_page_action_controller.h"

#include "url/gurl.h"

namespace commerce {

CommercePageActionController::CommercePageActionController(
    base::RepeatingCallback<void()> host_update_callback)
    : host_update_callback_(host_update_callback) {}

CommercePageActionController::~CommercePageActionController() = default;

void CommercePageActionController::NotifyHost() {
  host_update_callback_.Run();
}

}  // namespace commerce
