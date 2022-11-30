// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_PROMPT_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_PROMPT_H_

namespace commerce {

// This is a platform-independent interface for prompting shopping related UIs.
class ShoppingPrompt {
 public:
  virtual void ShowDiscountConsent() = 0;
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_PROMPT_H_
