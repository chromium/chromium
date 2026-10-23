// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_action_context_menu_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionActionContextMenuBridge_jni.h"

using base::android::JavaRef;
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
  CHECK(extension, base::NotFatalUntil::M162);

  auto* container = ExtensionsContainer::From(*browser);
  CHECK(container);
  ToolbarActionViewModel* action_model = container->GetActionForId(action_id);
  CHECK(action_model);

  ui::MenuModel* menu_model = action_model->GetContextMenu(context_menu_source);
  CHECK(menu_model);
  context_menu_model_ = menu_model->AsWeakPtr();

  menu_model_bridge_ =
      std::make_unique<ui::MenuModelBridge>(context_menu_model_);
}

ExtensionActionContextMenuBridge::~ExtensionActionContextMenuBridge() = default;

ScopedJavaLocalRef<jobject>
ExtensionActionContextMenuBridge::GetMenuModelBridge(JNIEnv* env) {
  return menu_model_bridge_->GetJavaObject().AsLocalRef(env);
}

static jni_zero::JniUniquePtr<ExtensionActionContextMenuBridge>
JNI_ExtensionActionContextMenuBridge_Init(
    JNIEnv* env,
    BrowserWindowInterface* browser,
    const ToolbarActionsModel::ActionId& action_id,
    content::WebContents* web_contents,
    ExtensionContextMenuModel::ContextMenuSource context_menu_source) {
  return jni_zero::MakeUnique<ExtensionActionContextMenuBridge>(
      browser, action_id, web_contents, context_menu_source);
}

}  // namespace extensions

DEFINE_JNI(ExtensionActionContextMenuBridge)
