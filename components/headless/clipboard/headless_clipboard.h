// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_CLIPBOARD_HEADLESS_CLIPBOARD_H_
#define COMPONENTS_HEADLESS_CLIPBOARD_HEADLESS_CLIPBOARD_H_

namespace headless {

// Set the default headless clipboard for the current thread.
void SetHeadlessClipboardForCurrentThread();

// Return sequence number request counter for the healess clipboard.
int GetSequenceNumberRequestCounterForTesting();

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_CLIPBOARD_HEADLESS_CLIPBOARD_H_
