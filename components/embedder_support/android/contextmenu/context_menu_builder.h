// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_CONTEXTMENU_CONTEXT_MENU_BUILDER_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_CONTEXTMENU_CONTEXT_MENU_BUILDER_H_

#include "base/android/scoped_java_ref.h"
#include "ui/menus/android/menu_model_bridge.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

namespace context_menu {

// Builds Java ContextMenuParams.
base::android::ScopedJavaGlobalRef<jobject> BuildJavaContextMenuParams(
    const content::ContextMenuParams& params,
    ui::MenuModel* menu_model,
    content::RenderFrameHost& initiator_frame_host);

content::ContextMenuParams* ContextMenuParamsFromJavaObject(
    const base::android::JavaRef<jobject>& jcontext_menu_params);

}  // namespace context_menu

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_CONTEXTMENU_CONTEXT_MENU_BUILDER_H_
