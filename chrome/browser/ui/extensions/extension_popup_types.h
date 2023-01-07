// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POPUP_TYPES_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POPUP_TYPES_H_

#include "base/functional/callback_forward.h"

namespace extensions {
class ExtensionHost;
}

enum class PopupShowAction {
  kShow,
  kShowAndInspect,
};

// A callback to run when a call to show a popup completes. If the popup is
// successfully shown, `popup_host` is the ExtensionHost for the popup. If the
// popup fails to show (e.g. due to the host closing before the popup can show),
// `popup_host` is null.
using ShowPopupCallback =
    base::OnceCallback<void(extensions::ExtensionHost* popup_host)>;

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POPUP_TYPES_H_
