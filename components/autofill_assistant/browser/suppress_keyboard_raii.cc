// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/suppress_keyboard_raii.h"

#include "base/bind.h"
#include "components/autofill/content/browser/content_autofill_driver.h"

namespace autofill_assistant {
namespace {

void SuppressKeyboardForFrame(bool suppress,
                              content::RenderFrameHost* render_frame_host) {
  auto* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(render_frame_host);
  if (driver != nullptr) {
    driver->GetAutofillAgent()->SetAssistantKeyboardSuppressState(suppress);
  }
}

}  // namespace

SuppressKeyboardRAII::SuppressKeyboardRAII(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  SuppressKeyboard(true);
}

SuppressKeyboardRAII::~SuppressKeyboardRAII() {
  SuppressKeyboard(false);
}

void SuppressKeyboardRAII::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  SuppressKeyboardForFrame(true, render_frame_host);
}

void SuppressKeyboardRAII::SuppressKeyboard(bool suppress) {
  web_contents()->GetMainFrame()->ForEachRenderFrameHost(
      base::BindRepeating(&SuppressKeyboardForFrame, suppress));
}

}  // namespace autofill_assistant
