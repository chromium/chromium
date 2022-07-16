// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SUPPRESS_KEYBOARD_RAII_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SUPPRESS_KEYBOARD_RAII_H_

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill_assistant {

// RAII object that suppresses the keyboard when the object is allocated and
// frees it when it gets deallocated.
class SuppressKeyboardRAII : public content::WebContentsObserver {
 public:
  SuppressKeyboardRAII(content::WebContents* web_contents);
  ~SuppressKeyboardRAII() override;

  SuppressKeyboardRAII(const SuppressKeyboardRAII&) = delete;
  SuppressKeyboardRAII& operator=(const SuppressKeyboardRAII&) = delete;

  // Overrides content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

 private:
  void SuppressKeyboard(bool suppress);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SUPPRESS_KEYBOARD_RAII_H_
