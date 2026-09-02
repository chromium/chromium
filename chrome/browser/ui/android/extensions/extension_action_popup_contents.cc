// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_action_popup_contents.h"

#include "base/android/jni_string.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionActionPopupContents_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
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
    std::unique_ptr<ExtensionViewHost> host,
    bool inspect_with_devtools,
    ShowPopupCallback callback)
    : host_(std::move(host)),
      inspect_with_devtools_(inspect_with_devtools),
      shown_callback_(std::move(callback)) {
  java_object_ = Java_ExtensionActionPopupContents_Constructor(
      AttachCurrentThread(), reinterpret_cast<int64_t>(this),
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

ExtensionActionPopupContents::~ExtensionActionPopupContents() {
  if (shown_callback_) {
    std::move(shown_callback_).Run(nullptr);
  }
}

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
  if (event.os_event.is_null()) {
    return false;
  }
  return Java_ExtensionActionPopupContents_handleKeyboardEvent(
      AttachCurrentThread(), java_object_, event.os_event);
}

void ExtensionActionPopupContents::OnLoaded() {
  if (shown_callback_) {
    std::move(shown_callback_).Run(host_.get());
  }
  if (inspect_with_devtools_) {
    DevToolsWindow::OpenDevToolsWindow(
        host_->host_contents(), DevToolsToggleAction::ShowConsolePanel(),
        DevToolsOpenedByAction::kContextMenuInspect);
  }
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


}  // namespace extensions

DEFINE_JNI(ExtensionActionPopupContents)
