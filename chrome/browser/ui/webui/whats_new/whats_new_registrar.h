// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_REGISTRAR_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_REGISTRAR_H_

#include "components/user_education/webui/whats_new_registry.h"

namespace whats_new {
namespace features {
BASE_DECLARE_FEATURE(kSafetyAwareness);
}  // namespace features

void RegisterWhatsNewModules(whats_new::WhatsNewRegistry* registry);

void RegisterWhatsNewEditions(whats_new::WhatsNewRegistry* registry);

std::unique_ptr<WhatsNewRegistry> CreateWhatsNewRegistry();
}  // namespace whats_new

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_REGISTRAR_H_
