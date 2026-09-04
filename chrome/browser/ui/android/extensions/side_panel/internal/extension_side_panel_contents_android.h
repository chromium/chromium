// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_SIDE_PANEL_INTERNAL_EXTENSION_SIDE_PANEL_CONTENTS_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_SIDE_PANEL_INTERNAL_EXTENSION_SIDE_PANEL_CONTENTS_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace extensions {

class ExtensionViewHost;

// ExtensionSidePanelContentsAndroid manages the native side of the extension
// side panel content on Android, bridging the ExtensionViewHost to the Java
// ExtensionSidePanelContents (which embeds the WebContents into a ThinWebView).
class ExtensionSidePanelContentsAndroid : public SidePanelNativeViewAndroid,
                                          public ExtensionView {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnViewDestroying() = 0;
  };

  ExtensionSidePanelContentsAndroid(ExtensionViewHost* host,
                                    ui::WindowAndroid* window_android);
  ExtensionSidePanelContentsAndroid(const ExtensionSidePanelContentsAndroid&) =
      delete;
  ExtensionSidePanelContentsAndroid& operator=(
      const ExtensionSidePanelContentsAndroid&) = delete;
  ~ExtensionSidePanelContentsAndroid() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ExtensionView:
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void OnLoaded() override;

 private:
  ExtensionSidePanelContentsAndroid(
      ExtensionViewHost* host,
      base::android::ScopedJavaLocalRef<jobject> java_object);

  raw_ptr<ExtensionViewHost> host_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_SIDE_PANEL_INTERNAL_EXTENSION_SIDE_PANEL_CONTENTS_ANDROID_H_
