// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_ASYNC_WRAPPER_IOS_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_ASYNC_WRAPPER_IOS_H_

#import <UIKit/UIKit.h>

#include "base/functional/callback.h"

using PasteboardCallback = base::OnceCallback<void(UIPasteboard*)>;

void GetGeneralPasteboard(bool asynchronous, PasteboardCallback callback);

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_ASYNC_WRAPPER_IOS_H_
