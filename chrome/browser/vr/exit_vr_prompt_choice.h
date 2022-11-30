// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_EXIT_VR_PROMPT_CHOICE_H_
#define CHROME_BROWSER_VR_EXIT_VR_PROMPT_CHOICE_H_

namespace vr {

// The answer the user gave to an exit VR prompt.
enum ExitVrPromptChoice {
  CHOICE_NONE,  // No answer give, e.g. user dismissed exit prompt or another
                // exit request was opened.
  CHOICE_EXIT,  // User wants to exit VR.
  CHOICE_STAY,  // User want to stay in VR.
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_EXIT_VR_PROMPT_CHOICE_H_
