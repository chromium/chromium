// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>

#include "chrome/browser/autofill/android/at_memory_bottom_sheet_delegate.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AtMemoryBottomSheetBridge::AtMemoryBottomSheetBridge(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AtMemoryBottomSheetBridge::~AtMemoryBottomSheetBridge() = default;

void AtMemoryBottomSheetBridge::RequestShowContent(
    std::unique_ptr<AtMemoryBottomSheetDelegate> delegate) {
  delegate_ = std::move(delegate);
  // TODO(crbug.com/502801668): Implement JNI call to show the bottom sheet.
}

void AtMemoryBottomSheetBridge::OnDismissed(JNIEnv* env) {
  if (delegate_) {
    delegate_->OnDismissed();
    ResetDelegate();
  }
}

void AtMemoryBottomSheetBridge::ResetDelegate() {
  delegate_.reset();
}

}  // namespace autofill
