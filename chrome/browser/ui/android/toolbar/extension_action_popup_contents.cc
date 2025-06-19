// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/extension_action_popup_contents.h"

#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/ExtensionActionPopupContents_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::RenderFrameHost;
using content::WebContents;

namespace extensions {

ExtensionActionPopupContents::ExtensionActionPopupContents(
    std::unique_ptr<ExtensionViewHost> host)
    : host_(std::move(host)) {
  java_object_ = Java_ExtensionActionPopupContents_Constructor(
      AttachCurrentThread(), reinterpret_cast<jlong>(this),
      host_->host_contents());
  host_->set_view(this);
}

ExtensionActionPopupContents::~ExtensionActionPopupContents() = default;

ScopedJavaLocalRef<jobject> ExtensionActionPopupContents::GetJavaObject() {
  return java_object_.AsLocalRef(AttachCurrentThread());
}

void ExtensionActionPopupContents::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  Java_ExtensionActionPopupContents_resizeDueToAutoResize(
      AttachCurrentThread(), java_object_, new_size.width(), new_size.height());
}

void ExtensionActionPopupContents::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  NOTIMPLEMENTED();
}

bool ExtensionActionPopupContents::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  NOTIMPLEMENTED();
  return false;
}

void ExtensionActionPopupContents::OnLoaded() {
  Java_ExtensionActionPopupContents_onLoaded(AttachCurrentThread(),
                                             java_object_);
}

void ExtensionActionPopupContents::Destroy(JNIEnv* env) {
  delete this;
}

void ExtensionActionPopupContents::LoadInitialPage(JNIEnv* env) {
  host_->CreateRendererSoon();
}

// JNI method to create an ExtensionActionPopupContents instance.
// This is called from the Java side to initiate the display of an extension
// popup.
static ScopedJavaLocalRef<jobject> JNI_ExtensionActionPopupContents_Create(
    JNIEnv* env,
    Profile* profile,
    std::string& action_id,
    int tab_id) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  DCHECK(registry);

  ExtensionActionManager* manager = ExtensionActionManager::Get(profile);
  DCHECK(manager);

  const Extension* extension =
      registry->enabled_extensions().GetByID(action_id);
  DCHECK(extension);

  ExtensionAction* action = manager->GetExtensionAction(*extension);
  DCHECK(action);

  GURL popup_url = action->GetPopupUrl(tab_id);

  std::unique_ptr<ExtensionViewHost> host =
      ExtensionViewHostFactory::CreatePopupHost(popup_url, profile);
  DCHECK(host);

  // The ExtensionActionPopupContents C++ object's lifetime is managed by its
  // Java counterpart. The Java object holds a pointer to this C++ instance.
  // When the Java side is finished with the popup, it will explicitly call
  // a 'destroy()' method on its Java object, which in turn calls the native
  // ExtensionActionPopupContents::Destroy() method, leading to the deletion
  // of this C++ object. Therefore, 'new' is used here, and ownership is
  // effectively passed to the Java-controlled lifecycle.
  ExtensionActionPopupContents* popup =
      new ExtensionActionPopupContents(std::move(host));
  return popup->GetJavaObject();
}

}  // namespace extensions
