// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/webid_dialog.h"

// Stub implementations for Identity UI on Android.

namespace content {
class WebContents;
}  // namespace content

// static
WebIdDialog* WebIdDialog::Create(content::WebContents* rp_web_contents) {
  NOTIMPLEMENTED();
  return nullptr;
}
