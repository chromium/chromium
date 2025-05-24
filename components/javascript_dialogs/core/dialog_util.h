// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_CORE_DIALOG_UTIL_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_CORE_DIALOG_UTIL_H_

#include <string>

#include "url/origin.h"

namespace javascript_dialogs::util {

std::u16string DialogTitle(const url::Origin& main_frame_origin,
                           const url::Origin& alerting_frame_origin);

}  // namespace javascript_dialogs::util

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_CORE_DIALOG_UTIL_H_
