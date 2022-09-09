// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_FACTORY_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_FACTORY_H_

#include <string>

#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "components/translate/core/common/translate_errors.h"

class BrowserWindow;
enum class ShowTranslateBubbleResult;

namespace content {
class WebContents;
}

// Factory to show the Full Page Translate bubble.
class TranslateBubbleFactory {
 public:
  virtual ~TranslateBubbleFactory();

  // Shows the Full Page Translate bubble. The behavior depends on the current
  // factory's implementation.
  static ShowTranslateBubbleResult Show(BrowserWindow* window,
                                        content::WebContents* web_contents,
                                        translate::TranslateStep step,
                                        const std::string& source_language,
                                        const std::string& target_language,
                                        translate::TranslateErrors error_type,
                                        bool is_user_gesture);

  // Sets the factory to change the behavior how to show the bubble.
  // TranslateBubbleFactory doesn't take the ownership of |factory|.
  static void SetFactory(TranslateBubbleFactory* factory);

 protected:
  // Shows the Full Page Translate bubble.
  virtual ShowTranslateBubbleResult ShowImplementation(
      BrowserWindow* window,
      content::WebContents* web_contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type) = 0;

 private:
  static TranslateBubbleFactory* current_factory_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_FACTORY_H_
