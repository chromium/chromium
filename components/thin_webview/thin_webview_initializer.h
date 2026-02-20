// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_
#define COMPONENTS_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_

#include "base/android/scoped_java_ref.h"
namespace content {
class WebContents;
}  // namespace content

namespace thin_webview {
namespace android {

// A helper class to help in attaching tab helpers and context menu populator.
class ThinWebViewInitializer {
 public:
  static void SetInstance(ThinWebViewInitializer* instance);
  static ThinWebViewInitializer* GetInstance();

  ThinWebViewInitializer() = default;

  ThinWebViewInitializer(const ThinWebViewInitializer&) = delete;
  ThinWebViewInitializer& operator=(const ThinWebViewInitializer&) = delete;

  ~ThinWebViewInitializer() = default;

  // Adopts the specified WebContents as a light version of a browser tab,
  // attaching all the associated tab helpers that are needed for the
  // WebContents to serve in that role.
  virtual void AttachTabHelpers(content::WebContents* web_contents) = 0;

  // Set the context menu populator factory for the web contents. This allows
  // the ThinWebView to show a context menu on long press.
  virtual void SetContextMenuPopulatorFactory(
      content::WebContents* web_contents,
      const base::android::JavaRef<jobject>& jpopulator_factory) = 0;
};

}  // namespace android
}  // namespace thin_webview

#endif  // COMPONENTS_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_
