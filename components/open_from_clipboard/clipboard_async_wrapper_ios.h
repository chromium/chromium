// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_ASYNC_WRAPPER_IOS_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_ASYNC_WRAPPER_IOS_H_

#import <UIKit/UIKit.h>

#include "base/functional/callback.h"

using PasteboardCallback = base::OnceCallback<void(UIPasteboard*)>;

// Asynchronously retrieves `UIPasteboard.generalPasteboard` on a
// `base::MayBlock()` sequenced task runner and invokes `callback` with the
// pasteboard on the calling sequence.
void GetGeneralPasteboard(PasteboardCallback callback);

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_ASYNC_WRAPPER_IOS_H_
