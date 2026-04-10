// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/test_support_jni_headers/ExtensionTestUtils_jni.h"

using base::FilePath;

namespace extensions {

static void JNI_ExtensionTestUtils_LoadUnpackedExtensionAsync(
    JNIEnv* env,
    Profile* profile,
    const std::string& root_dir,
    const base::android::JavaRef<jobject>& callback) {
  ChromeTestExtensionLoader loader(profile);
  loader.LoadUnpackedExtensionAsync(
      FilePath::FromUTF8Unsafe(root_dir),
      base::BindOnce(
          [](const base::android::JavaRef<jobject>& callback,
             const Extension* extension) {
            base::android::RunStringCallbackAndroid(callback, extension->id());
          },
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

static void JNI_ExtensionTestUtils_EnableExtension(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id) {
  extensions::ExtensionRegistrar::Get(profile)->EnableExtension(extension_id);
}

static void JNI_ExtensionTestUtils_DisableExtension(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id) {
  extensions::ExtensionRegistrar::Get(profile)->DisableExtension(
      extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
}

static void JNI_ExtensionTestUtils_UninstallExtension(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id) {
  extensions::ExtensionRegistrar::Get(profile)->UninstallExtension(
      extension_id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
}

static void JNI_ExtensionTestUtils_SetExtensionActionVisible(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id,
    bool visible) {
  ToolbarActionsModel::Get(profile)->SetActionVisibility(extension_id, visible);
}

static std::vector<ToolbarActionsModel::ActionId>
JNI_ExtensionTestUtils_GetPinnedActionIds(JNIEnv* env, Profile* profile) {
  return ToolbarActionsModel::Get(profile)->pinned_action_ids();
}

static jint JNI_ExtensionTestUtils_GetRenderFrameHostCount(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id) {
  return extensions::ProcessManager::Get(profile)
      ->GetRenderFrameHostsForExtension(extension_id)
      .size();
}

static void JNI_ExtensionTestUtils_SetWithholdHostPermissions(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id,
    jboolean withhold) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_id);
  extensions::ScriptingPermissionsModifier(profile,
                                           base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(withhold);
}

static void JNI_ExtensionTestUtils_AddHostAccessRequest(
    JNIEnv* env,
    Profile* profile,
    content::WebContents* web_contents,
    const std::string& extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_id);
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  extensions::PermissionsManager::Get(profile)->AddHostAccessRequest(
      web_contents, tab_id, *extension);
}

static jboolean JNI_ExtensionTestUtils_HasGrantedHostPermission(
    JNIEnv* env,
    Profile* profile,
    const std::string& extension_id,
    const std::string& url) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_id);
  if (!extension) {
    return false;
  }
  return extensions::PermissionsManager::Get(profile)->HasGrantedHostPermission(
      *extension, GURL(url));
}

}  // namespace extensions

DEFINE_JNI(ExtensionTestUtils)
