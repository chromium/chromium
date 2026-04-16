// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/contextual_tasks/search_ai_mode_signin_promo_controller_observer.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

class SearchAIModeSignInPromoView;
class BrowserView;

class SearchAIModeSignInPromoController {
 public:
  using Observer =
      ::contextual_tasks::SearchAIModeSignInPromoControllerObserver;

  explicit SearchAIModeSignInPromoController(
      content::WebContents* web_contents);
  virtual ~SearchAIModeSignInPromoController();
  SearchAIModeSignInPromoController(const SearchAIModeSignInPromoController&) =
      delete;
  SearchAIModeSignInPromoController& operator=(
      const SearchAIModeSignInPromoController&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Triggers the promo, subject to eligibility conditions (rate limits).
  // Returns false is the flow is aborted and the promo cannot be shown,
  // otherwise it returns true and triggers the promo.
  virtual bool MaybeShowPromo(BrowserView* browser_view);

  // Called once when the view is being destroyed.
  // It resets temporary UI state.
  void OnViewIsDeleting();

  // Called when the promo bubble starts to close. Determines if the sign-in
  // flow should be aborted based on the `closed_reason`.
  void HandlePromoClosing(views::Widget::ClosedReason closed_reason);

  base::WeakPtr<SearchAIModeSignInPromoController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<SearchAIModeSignInPromoView> promo_view_ = nullptr;
  base::ObserverList<Observer> observers_;
  base::ScopedClosureRunner avatar_pill_closure_runner_;
  base::WeakPtrFactory<SearchAIModeSignInPromoController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_
