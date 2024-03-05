// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_password_autofill_agent.h"

namespace autofill {

TestPasswordAutofillAgent::TestPasswordAutofillAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : PasswordAutofillAgent(render_frame,
                            registry,
                            EnableHeavyFormDataScraping(false)) {}

TestPasswordAutofillAgent::~TestPasswordAutofillAgent() = default;

bool TestPasswordAutofillAgent::FrameCanAccessPasswordManager() {
  return true;
}

}  // namespace autofill
