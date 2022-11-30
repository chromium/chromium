// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fake_delegate.h"

gfx::NativeView FakeDelegate::GetNativeView() {
  return gfx::kNullNativeView;
}

content::WebContents* FakeDelegate::GetWebContents() {
  return web_contents_;
}
