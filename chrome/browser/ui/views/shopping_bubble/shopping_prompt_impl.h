// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHOPPING_BUBBLE_SHOPPING_PROMPT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_SHOPPING_BUBBLE_SHOPPING_PROMPT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/commerce/core/shopping_prompt.h"

class LocationBarView;

namespace content {
class WebContents;
}  // namespace content

// This is the Desktop implementation. It will trigger UI to reflate additional
// shopping related info on the page/site.
class ShoppingPromptImpl : public commerce::ShoppingPrompt {
 public:
  explicit ShoppingPromptImpl(content::WebContents* web_contents);

  void ShowDiscountConsent() override;

 private:
  LocationBarView* GetLocationBarView();

  // The web contents whose location bar should show the prompt.
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHOPPING_BUBBLE_SHOPPING_PROMPT_IMPL_H_
