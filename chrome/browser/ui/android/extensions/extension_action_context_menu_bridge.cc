// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_action_context_menu_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionActionContextMenuBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;
using extensions::Extension;
using extensions::ExtensionContextMenuModel;
using extensions::ExtensionRegistry;

namespace extensions {

ExtensionActionContextMenuBridge::ExtensionActionContextMenuBridge(
    BrowserWindowInterface* browser,
    const ToolbarActionsModel::ActionId& action_id,
    content::WebContents* web_contents,
    ExtensionContextMenuModel::ContextMenuSource context_menu_source) {
  Profile* profile = browser->GetProfile();
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  scoped_refptr<const extensions::Extension> extension =
      registry->enabled_extensions().GetByID(action_id);
  DCHECK(extension);

  bool is_pinned =
      ToolbarActionsModel::Get(profile)->IsActionPinned(extension->id());

  extension_context_menu_model_ = std::make_unique<ExtensionContextMenuModel>(
      extension.get(), browser, is_pinned, /* delegate= */ nullptr,
      /* can_show_icon_in_toolbar= */ true, context_menu_source);

  menu_model_bridge_ = std::make_unique<ui::MenuModelBridge>(
      extension_context_menu_model_->AsWeakPtr());
}

ExtensionActionContextMenuBridge::~ExtensionActionContextMenuBridge() {}

ScopedJavaLocalRef<jobject>
ExtensionActionContextMenuBridge::GetMenuModelBridge(JNIEnv* env) {
  return menu_model_bridge_->GetJavaObject().AsLocalRef(env);
}

void ExtensionActionContextMenuBridge::Destroy(JNIEnv* env) {
  delete this;
}

static jlong JNI_ExtensionActionContextMenuBridge_Init(
    JNIEnv* env,
    jlong browser_window_interface_ptr,
    ToolbarActionsModel::ActionId& action_id,
    content::WebContents* web_contents,
    ExtensionContextMenuModel::ContextMenuSource context_menu_source) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(browser_window_interface_ptr);
  auto* bridge = new ExtensionActionContextMenuBridge(
      browser, action_id, web_contents, context_menu_source);
  // The bridge is owned by the Java object and will be destroyed when
  // ExtensionActionContextMenuBridge.destroy() is called.
  return reinterpret_cast<jlong>(bridge);
}

}  // namespace extensions

DEFINE_JNI(ExtensionActionContextMenuBridge)
