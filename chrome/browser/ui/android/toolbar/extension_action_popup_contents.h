// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_EXTENSION_ACTION_POPUP_CONTENTS_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_EXTENSION_ACTION_POPUP_CONTENTS_H_

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/extensions/extension_view.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class ExtensionViewHost;

// ExtensionActionPopupContents is the native C++ class responsible for managing
// the content of an extension's popup displayed on Android. An extension popup
// is typically a small HTML page an extension can show when its action icon
// is clicked. This class bridges the C++ extensions system with the Java UI.
//
// Lifetime Management:
// An instance of this C++ class is created when its Java counterpart
// (ExtensionActionPopupContents.java) requests it via a JNI call (specifically,
// JNI_ExtensionActionPopupContents_Create). The C++ object's lifetime is tied
// to its Java peer. The Java object holds a native pointer (jlong) to this C++
// instance. When the Java object is no longer needed (e.g. the popup is
// closed), its `destroy()` method is called. This, in turn, calls the native
// `Destroy()` method on this C++ object, which then calls `delete this`.
class ExtensionActionPopupContents : public ExtensionView {
 public:
  explicit ExtensionActionPopupContents(
      std::unique_ptr<ExtensionViewHost> popup_host);
  ExtensionActionPopupContents(const ExtensionActionPopupContents&) = delete;
  ExtensionActionPopupContents& operator=(const ExtensionActionPopupContents&) =
      delete;
  ~ExtensionActionPopupContents() override;

  // Returns a local JNI reference to the Java counterpart of this object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // ExtensionView:
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void OnLoaded() override;

  // Called from Java when the Java counterpart is being destroyed.
  void Destroy(JNIEnv* env);

  // Called from Java to trigger the loading of the popup's initial URL in the
  // hosted WebContents.
  void LoadInitialPage(JNIEnv* env);

 private:
  std::unique_ptr<ExtensionViewHost> host_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_EXTENSION_ACTION_POPUP_CONTENTS_H_
