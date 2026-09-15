// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extension_tab_context_menu_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "content/public/browser/web_contents.h"
#include "ui/menus/android/menu_model_bridge.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_menu_model_android.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ExtensionTabContextMenuBridge_jni.h"

using base::android::ScopedJavaLocalRef;

namespace extensions {

ExtensionTabContextMenuBridge::ExtensionTabContextMenuBridge(
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  CHECK(web_contents);
  menu_model_ = std::make_unique<ExtensionMenuModel>(
      web_contents->GetBrowserContext(), web_contents);
  menu_model_->PopulateModel();
  if (menu_model_->HasVisibleItems()) {
    menu_model_bridge_ =
        std::make_unique<ui::MenuModelBridge>(menu_model_->AsWeakPtr());
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

ExtensionTabContextMenuBridge::~ExtensionTabContextMenuBridge() = default;

ScopedJavaLocalRef<jobject> ExtensionTabContextMenuBridge::GetMenuModelBridge(
    JNIEnv* env) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return menu_model_bridge_
             ? menu_model_bridge_->GetJavaObject().AsLocalRef(env)
             : nullptr;
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

void ExtensionTabContextMenuBridge::Destroy(JNIEnv* env) {
  delete this;
}

bool ExtensionTabContextMenuBridge::IsEmpty() const {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return !menu_model_bridge_;
#else
  return true;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

static int64_t JNI_ExtensionTabContextMenuBridge_Init(
    JNIEnv* env,
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!web_contents) {
    return 0;
  }
  auto bridge = std::make_unique<ExtensionTabContextMenuBridge>(web_contents);
  if (bridge->IsEmpty()) {
    return 0;
  }
  return reinterpret_cast<int64_t>(bridge.release());
#else
  return 0;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

}  // namespace extensions

DEFINE_JNI(ExtensionTabContextMenuBridge)
