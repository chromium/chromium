// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_CONTEXTMENU_CONTEXT_MENU_BUILDER_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_CONTEXTMENU_CONTEXT_MENU_BUILDER_H_

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"

namespace content {
struct ContextMenuParams;
}

namespace context_menu {

base::android::ScopedJavaGlobalRef<jobject> BuildJavaContextMenuParams(
    const content::ContextMenuParams& params,
    int initiator_process_id = 0,
    std::optional<base::UnguessableToken> initiator_frame_token = std::nullopt);

content::ContextMenuParams* ContextMenuParamsFromJavaObject(
    const base::android::JavaRef<jobject>& jcontext_menu_params);

}  // namespace context_menu

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_CONTEXTMENU_CONTEXT_MENU_BUILDER_H_
