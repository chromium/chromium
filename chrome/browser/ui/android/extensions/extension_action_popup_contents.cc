// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_action_popup_contents.h"

#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionActionPopupContents_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::RenderFrameHost;
using content::WebContents;

namespace extensions {

namespace {

// The minimum and maximum sizes for the extension popup.
// https://developer.chrome.com/docs/extensions/reference/api/action#popup
constexpr gfx::Size kMinSize = {25, 25};
constexpr gfx::Size kMaxSize = {800, 600};

}  // namespace

ExtensionActionPopupContents::ExtensionActionPopupContents(
    std::unique_ptr<ExtensionViewHost> host)
    : host_(std::move(host)) {
  java_object_ = Java_ExtensionActionPopupContents_Constructor(
      AttachCurrentThread(), reinterpret_cast<jlong>(this),
      host_->host_contents());
  host_->set_view(this);
  // Handle the containing view calling window.close();
  // The base::Unretained() below is safe because this object owns `host_`, so
  // the callback will never fire if `this` is deleted.
  host_->SetCloseHandler(
      base::BindOnce(&ExtensionActionPopupContents::HandleCloseExtensionHost,
                     base::Unretained(this)));
  WebContentsObserver::Observe(host_->host_contents());
  auto* primary_main_frame = host_->host_contents()->GetPrimaryMainFrame();
  if (primary_main_frame->IsRenderFrameLive()) {
    SetUpNewMainFrame(primary_main_frame);
  }
}

ExtensionActionPopupContents::~ExtensionActionPopupContents() = default;

ScopedJavaLocalRef<jobject> ExtensionActionPopupContents::GetJavaObject() {
  return java_object_.AsLocalRef(AttachCurrentThread());
}

void ExtensionActionPopupContents::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  // Since we skipped speculative main frames in RenderFrameCreated, we must
  // watch for them being swapped in by watching for RenderFrameHostChanged().
  if (new_host != host_->host_contents()->GetPrimaryMainFrame()) {
    return;
  }

  // Ignore the initial main frame host, as there's no renderer frame for it
  // yet. If the DCHECK fires, then we would need to handle the initial main
  // frame when it its renderer frame is created.
  if (!old_host) {
    DCHECK(!new_host->IsRenderFrameLive());
    return;
  }

  SetUpNewMainFrame(new_host);
}

void ExtensionActionPopupContents::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  Java_ExtensionActionPopupContents_resizeDueToAutoResize(
      AttachCurrentThread(), java_object_, new_size.width(), new_size.height());
}

void ExtensionActionPopupContents::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Only handle the initial main frame, not speculative ones.
  if (render_frame_host != host_->host_contents()->GetPrimaryMainFrame()) {
    return;
  }

  SetUpNewMainFrame(render_frame_host);
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

void ExtensionActionPopupContents::SetUpNewMainFrame(
    RenderFrameHost* render_frame_host) {
  render_frame_host->GetView()->EnableAutoResize(kMinSize, kMaxSize);
}

void ExtensionActionPopupContents::HandleCloseExtensionHost(
    ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  Java_ExtensionActionPopupContents_onClose(AttachCurrentThread(),
                                            java_object_);
}

// JNI method to create an ExtensionActionPopupContents instance.
// This is called from the Java side to initiate the display of an extension
// popup.
static ScopedJavaLocalRef<jobject> JNI_ExtensionActionPopupContents_Create(
    JNIEnv* env,
    jlong browser_window_interface_ptr,
    std::string& action_id,
    int tab_id) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(browser_window_interface_ptr);
  Profile* profile = browser->GetProfile();

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
      ExtensionViewHostFactory::CreatePopupHost(popup_url, browser);
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

DEFINE_JNI(ExtensionActionPopupContents)
