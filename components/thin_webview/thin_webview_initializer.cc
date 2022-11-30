// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/thin_webview/thin_webview_initializer.h"

namespace thin_webview {
namespace android {

ThinWebViewInitializer* g_thin_webview_initializer = nullptr;

// static
void ThinWebViewInitializer::SetInstance(ThinWebViewInitializer* instance) {
  g_thin_webview_initializer = instance;
}

// static
ThinWebViewInitializer* ThinWebViewInitializer::GetInstance() {
  return g_thin_webview_initializer;
}

}  // namespace android
}  // namespace thin_webview
