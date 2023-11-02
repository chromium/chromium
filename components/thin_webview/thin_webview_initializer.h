// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_
#define COMPONENTS_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_

namespace content {
class WebContents;
}  // namespace content

namespace thin_webview {
namespace android {

// A helper class to help in attaching tab helpers.
class ThinWebViewInitializer {
 public:
  static void SetInstance(ThinWebViewInitializer* instance);
  static ThinWebViewInitializer* GetInstance();

  ThinWebViewInitializer() = default;

  ThinWebViewInitializer(const ThinWebViewInitializer&) = delete;
  ThinWebViewInitializer& operator=(const ThinWebViewInitializer&) = delete;

  ~ThinWebViewInitializer() = default;

  virtual void AttachTabHelpers(content::WebContents* web_contents) = 0;
};

}  // namespace android
}  // namespace thin_webview

#endif  // COMPONENTS_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_
