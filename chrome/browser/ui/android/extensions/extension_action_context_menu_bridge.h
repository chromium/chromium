// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "ui/menus/android/menu_model_bridge.h"

class BrowserWindowInterface;

namespace extensions {
// This class is responsible for managing `ExtensionContextMenuModel` and
// `MenuModelBridge`, both of which are needed to display the context menu on
// toolbar actions.
//
// Lifetime management:
// An instance of this C++ class is created when its Java counterpart
// (ExtensionActionContextMenuBridge.java) requests it via a JNI call. The C++
// object's lifetime is tied to its Java peer, and when the Java object is no
// longer needed, its `destroy()` method is called, which in turn calls the
// native `Destroy()` method on this C++ object, which then calls `delete this`.
class ExtensionActionContextMenuBridge {
 public:
  // Constructs a bridge for creating an Android context menu for an extension
  // action. `action_id` is the ID of the extension, and `web_contents` is the
  // WebContents currently on display. `context_menu_source` indicates whether
  // the context menu was opened from the toolbar or inside the extensions menu.
  ExtensionActionContextMenuBridge(
      BrowserWindowInterface* browser,
      const ToolbarActionsModel::ActionId& action_id,
      content::WebContents* web_contents,
      ExtensionContextMenuModel::ContextMenuSource context_menu_source);

  ExtensionActionContextMenuBridge(const ExtensionActionContextMenuBridge&) =
      delete;
  ExtensionActionContextMenuBridge& operator=(
      const ExtensionActionContextMenuBridge&) = delete;

  ~ExtensionActionContextMenuBridge();

  jni_zero::ScopedJavaLocalRef<jobject> GetMenuModelBridge(JNIEnv* env);

  void Destroy(JNIEnv* env);

 private:
  std::unique_ptr<ExtensionContextMenuModel> extension_context_menu_model_;
  std::unique_ptr<ui::MenuModelBridge> menu_model_bridge_;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_BRIDGE_H_
