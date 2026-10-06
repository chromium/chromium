// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_POPUP_CONTENTS_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_POPUP_CONTENTS_H_

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class ExtensionHost;
class ExtensionViewHost;

// ExtensionActionPopupContents is the native C++ class responsible for managing
// the content of an extension's popup displayed on Android. An extension popup
// is typically a small HTML page an extension can show when its action icon
// is clicked. This class bridges the C++ extensions system with the Java UI.
//
// Lifetime Management:
// Create() constructs this object and hands it to its Java peer
// (ExtensionActionPopupContents.java) as a JniUniquePtr. The Java peer owns
// this C++ instance and deletes it when `destroy()` is called.
class ExtensionActionPopupContents : public content::WebContentsObserver,
                                     public ExtensionView {
 public:
  // Returns the Java peer, which owns the created native object.
  static base::android::ScopedJavaLocalRef<jobject> Create(
      std::unique_ptr<ExtensionViewHost> popup_host,
      bool inspect_with_devtools,
      ShowPopupCallback callback = ShowPopupCallback());

  ExtensionActionPopupContents(std::unique_ptr<ExtensionViewHost> popup_host,
                               bool inspect_with_devtools,
                               ShowPopupCallback callback);
  ExtensionActionPopupContents(const ExtensionActionPopupContents&) = delete;
  ExtensionActionPopupContents& operator=(const ExtensionActionPopupContents&) =
      delete;
  ~ExtensionActionPopupContents() override;

  // WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // ExtensionView:
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void OnLoaded() override;

  // Called from Java to trigger the loading of the popup's initial URL in the
  // hosted WebContents.
  void LoadInitialPage();

 private:
  // Starts receiving callbacks from `host_`. Deferred until `java_object_` is
  // set because those callbacks are forwarded to Java.
  void AttachToHost();
  void SetUpNewMainFrame(content::RenderFrameHost* render_frame_host);
  void HandleCloseExtensionHost(extensions::ExtensionHost* host);

  std::unique_ptr<ExtensionViewHost> host_;
  const bool inspect_with_devtools_;
  ShowPopupCallback shown_callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTION_POPUP_CONTENTS_H_
