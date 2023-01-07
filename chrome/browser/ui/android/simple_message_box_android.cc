// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"

namespace chrome {

MessageBoxResult ShowWarningMessageBox(gfx::NativeWindow parent,
                                       const std::u16string& title,
                                       const std::u16string& message) {
  NOTIMPLEMENTED();
  return MESSAGE_BOX_RESULT_NO;
}

MessageBoxResult ShowQuestionMessageBox(gfx::NativeWindow parent,
                                        const std::u16string& title,
                                        const std::u16string& message) {
  NOTIMPLEMENTED();
  return MESSAGE_BOX_RESULT_NO;
}

void ShowWarningMessageBoxWithCheckbox(
    gfx::NativeWindow parent,
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& checkbox_text,
    base::OnceCallback<void(bool checked)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

}  // namespace chrome
