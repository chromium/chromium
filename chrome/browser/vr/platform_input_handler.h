// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_PLATFORM_INPUT_HANDLER_H_
#define CHROME_BROWSER_VR_PLATFORM_INPUT_HANDLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/vr/text_edit_action.h"

namespace vr {

class InputEvent;

typedef typename base::OnceCallback<void(const std::u16string&)>
    TextStateUpdateCallback;

// This class defines input related interfaces which each platform should
// implement its own.
class PlatformInputHandler {
 public:
  virtual ~PlatformInputHandler() {}
  virtual void ForwardEventToPlatformUi(std::unique_ptr<InputEvent> event) = 0;
  virtual void ForwardEventToContent(std::unique_ptr<InputEvent> event,
                                     int content_id) = 0;

  // Text input specific.
  virtual void ClearFocusedElement() = 0;
  virtual void OnWebInputEdited(const TextEdits& edits) = 0;
  virtual void SubmitWebInput() = 0;
  virtual void RequestWebInputText(TextStateUpdateCallback callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_PLATFORM_INPUT_HANDLER_H_
