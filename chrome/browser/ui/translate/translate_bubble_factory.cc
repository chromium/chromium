// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_factory.h"

#include <string>

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/translate/core/browser/translate_step.h"

namespace {

ShowTranslateBubbleResult ShowDefault(BrowserWindow* window,
                                      content::WebContents* web_contents,
                                      translate::TranslateStep step,
                                      const std::string& source_language,
                                      const std::string& target_language,
                                      translate::TranslateErrors error_type,
                                      bool is_user_gesture) {
  // |window| might be null when testing.
  if (!window)
    return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID;
  return window->ShowTranslateBubble(web_contents, step, source_language,
                                     target_language, error_type,
                                     is_user_gesture);
}

}  // namespace

TranslateBubbleFactory::~TranslateBubbleFactory() {
}

// static
ShowTranslateBubbleResult TranslateBubbleFactory::Show(
    BrowserWindow* window,
    content::WebContents* web_contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  if (current_factory_) {
    return current_factory_->ShowImplementation(window, web_contents, step,
                                                source_language,
                                                target_language, error_type);
  }

  return ShowDefault(window, web_contents, step, source_language,
                     target_language, error_type, is_user_gesture);
}

// static
void TranslateBubbleFactory::SetFactory(TranslateBubbleFactory* factory) {
  current_factory_ = factory;
}

// static
TranslateBubbleFactory* TranslateBubbleFactory::current_factory_ = nullptr;
