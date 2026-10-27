// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_CONTROLLER_H_

#include <optional>
#include <string>

#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_anchor.h"
#endif

class BrowserWindowInterface;
class PartialTranslateBubbleView;
class TranslateBubbleView;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

// A cross-platform interface to control Translate UI, registered as
// UnownedUserData on BrowserWindowInterface.
class TranslateController {
 public:
  DECLARE_USER_DATA(TranslateController);

  virtual ~TranslateController() = default;

  static TranslateController* From(BrowserWindowInterface* window);

#if defined(TOOLKIT_VIEWS)
  virtual views::Widget* ShowTranslateBubble(
      content::WebContents* web_contents,
      views::BubbleAnchor anchor,
      std::optional<ui::ElementIdentifier> highlighted_element,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      bool is_user_gesture) = 0;
#endif

  virtual void StartPartialTranslate(const std::string& source_language,
                                     const std::string& target_language,
                                     const std::u16string& text_selection) = 0;

  virtual void CloseBubble() = 0;

  virtual TranslateBubbleView* GetTranslateBubble() const = 0;

  virtual PartialTranslateBubbleView* GetPartialTranslateBubble() const = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_CONTROLLER_H_
