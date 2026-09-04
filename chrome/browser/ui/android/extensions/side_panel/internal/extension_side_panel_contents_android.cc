// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/side_panel/internal/extension_side_panel_contents_android.h"

#include "base/android/jni_android.h"
#include "base/check.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/side_panel/internal/jni/ExtensionSidePanelContents_jni.h"

namespace extensions {

namespace {

base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
    ExtensionViewHost* host,
    ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ExtensionSidePanelContents_create(
      env, host->host_contents(), window_android->GetJavaObject());
}

base::android::ScopedJavaLocalRef<jobject> GetViewFromJavaObject(
    const base::android::JavaRef<jobject>& java_obj) {
  if (!java_obj) {
    return nullptr;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ExtensionSidePanelContents_getView(env, java_obj);
}

}  // namespace

ExtensionSidePanelContentsAndroid::ExtensionSidePanelContentsAndroid(
    ExtensionViewHost* host,
    ui::WindowAndroid* window_android)
    : ExtensionSidePanelContentsAndroid(
          host,
          CreateJavaObject(host, window_android)) {}

ExtensionSidePanelContentsAndroid::ExtensionSidePanelContentsAndroid(
    ExtensionViewHost* host,
    base::android::ScopedJavaLocalRef<jobject> java_object)
    : SidePanelNativeViewAndroid(GetViewFromJavaObject(java_object)),
      host_(host),
      java_object_(java_object) {
  CHECK(host_);
  // `java_object_` may be null if Java-side initialization failed (e.g.
  // uninitialized window context) or in tests.
  if (java_object_) {
    host_->set_view(this);
    host_->CreateRendererSoon();
  }
}

ExtensionSidePanelContentsAndroid::~ExtensionSidePanelContentsAndroid() {
  if (java_object_) {
    host_->set_view(nullptr);
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ExtensionSidePanelContents_destroy(env, java_object_);
  }
  for (auto& observer : observers_) {
    observer.OnViewDestroying();
  }
}

void ExtensionSidePanelContentsAndroid::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionSidePanelContentsAndroid::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// ExtensionView overrides:
void ExtensionSidePanelContentsAndroid::ResizeDueToAutoResize(
    content::WebContents* /*web_contents*/,
    const gfx::Size& /*new_size*/) {
  // Side panels are sized and managed by the browser UI container rather than
  // auto-resizing to content.
}

void ExtensionSidePanelContentsAndroid::RenderFrameCreated(
    content::RenderFrameHost* /*render_frame_host*/) {
  // No frame-specific setup needed for side panels.
}

bool ExtensionSidePanelContentsAndroid::HandleKeyboardEvent(
    content::WebContents* /*source*/,
    const input::NativeWebKeyboardEvent& /*event*/) {
  // Unhandled keyboard events fall back to default Android window event
  // routing.
  return false;
}

void ExtensionSidePanelContentsAndroid::OnLoaded() {
  // Side panel visibility and `onOpened` dispatch are handled by
  // ExtensionSidePanelCoordinator.
}

}  // namespace extensions
