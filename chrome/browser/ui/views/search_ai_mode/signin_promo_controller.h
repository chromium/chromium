// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"

class SearchAIModeSignInPromoView;
class BrowserView;

class SearchAIModeSignInPromoController {
 public:
  explicit SearchAIModeSignInPromoController(
      content::WebContents* web_contents);
  ~SearchAIModeSignInPromoController();
  SearchAIModeSignInPromoController(const SearchAIModeSignInPromoController&) =
      delete;
  SearchAIModeSignInPromoController& operator=(
      const SearchAIModeSignInPromoController&) = delete;

  // Triggers the promo.
  void ShowPromo(BrowserView* browser_view);

  // Called when the bubble is closed.
  void OnBubbleClosed();

  base::WeakPtr<SearchAIModeSignInPromoController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<SearchAIModeSignInPromoView> promo_view_ = nullptr;
  base::ScopedClosureRunner avatar_pill_closure_runner_;
  base::WeakPtrFactory<SearchAIModeSignInPromoController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_
