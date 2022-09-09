// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_COMMERCE_PROMPT_H_
#define CHROME_BROWSER_UI_COMMERCE_COMMERCE_PROMPT_H_

#include "chrome/browser/cart/chrome_cart.mojom.h"

class Browser;

namespace commerce {
// Factory function to create and show the discount consent prompts for chrome.
void ShowDiscountConsentPrompt(
    Browser* browser,
    base::OnceCallback<void(chrome_cart::mojom::ConsentStatus)> callback);
}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_COMMERCE_PROMPT_H_
