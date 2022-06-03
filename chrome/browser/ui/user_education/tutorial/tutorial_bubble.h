// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_H_

// TutorialBubble is an interface for the lifecycle of a tutorial bubble
// it is implemented by a framework's bubble. It is returned as the result of
// TutorialBubbleFactory's CreateBubble method.
struct TutorialBubble {
  TutorialBubble() = default;
  TutorialBubble(const TutorialBubble&) = delete;
  TutorialBubble& operator=(const TutorialBubble&) = delete;
  virtual ~TutorialBubble() = default;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_H_
