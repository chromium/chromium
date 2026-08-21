// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/contextual_tasks/search_ai_mode_signin_promo_controller_observer.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

class AvatarToolbarButtonInterface;
class BrowserView;
class BrowserWindowInterface;
class Profile;
class AIModeSignInPromoViewBase;

// Base controller for managing the lifecycle and presentation of the location
// bar sign-in promo bubble.
class AIModeSignInPromoControllerBase {
 public:
  explicit AIModeSignInPromoControllerBase(
      content::WebContents* web_contents,
      signin_metrics::AccessPoint access_point);
  virtual ~AIModeSignInPromoControllerBase();
  AIModeSignInPromoControllerBase(const AIModeSignInPromoControllerBase&) =
      delete;
  AIModeSignInPromoControllerBase& operator=(
      const AIModeSignInPromoControllerBase&) = delete;

  // Triggers the promo, subject to eligibility conditions (rate limits).
  // Returns false if the flow is aborted and the promo cannot be shown,
  // otherwise returns true and triggers the promo.
  virtual bool MaybeShowPromo(BrowserView* browser_view);

  // Called once when the view is being destroyed.
  // It resets temporary UI state.
  void OnViewIsDeleting();

  // Called when the promo bubble starts to close. Determines if the sign-in
  // flow should be aborted based on the `closed_reason`.
  void HandlePromoClosing(views::Widget::ClosedReason closed_reason);

  signin_metrics::AccessPoint access_point() const { return access_point_; }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

  virtual bool CanShowPromo(Profile& profile) = 0;
  virtual void UpdateAvatarButtonState(
      AvatarToolbarButtonInterface& avatar_button) {}
  virtual void OnViewDeleted() {}
  virtual void OnPromoIneligible() {}
  virtual void OnPromoDismissedOrAborted() {}
  virtual std::unique_ptr<AIModeSignInPromoViewBase> CreatePromoView(
      views::BubbleAnchor anchor) = 0;

 private:
  base::WeakPtr<content::WebContents> web_contents_;
  signin_metrics::AccessPoint access_point_;
  raw_ptr<AIModeSignInPromoViewBase> promo_view_ = nullptr;
};

// Controller for the Search AI Mode sign-in promo bubble.
class SearchAIModeSignInPromoController
    : public AIModeSignInPromoControllerBase {
 public:
  using Observer =
      ::contextual_tasks::SearchAIModeSignInPromoControllerObserver;

  explicit SearchAIModeSignInPromoController(
      content::WebContents* web_contents);
  ~SearchAIModeSignInPromoController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::WeakPtr<SearchAIModeSignInPromoController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  bool CanShowPromo(Profile& profile) override;
  void UpdateAvatarButtonState(
      AvatarToolbarButtonInterface& avatar_button) override;
  void OnViewDeleted() override;
  void OnPromoIneligible() override;
  void OnPromoDismissedOrAborted() override;
  std::unique_ptr<AIModeSignInPromoViewBase> CreatePromoView(
      views::BubbleAnchor anchor) override;

 private:
  base::ObserverList<Observer> observers_;
  base::ScopedClosureRunner avatar_pill_closure_runner_;
  base::WeakPtrFactory<SearchAIModeSignInPromoController> weak_ptr_factory_{
      this};
};

// Controller for the Composebox Drive context menu option sign-in promo bubble.
class ComposeboxDriveSignInPromoController
    : public AIModeSignInPromoControllerBase {
 public:
  explicit ComposeboxDriveSignInPromoController(
      content::WebContents* web_contents);
  ~ComposeboxDriveSignInPromoController() override;

  using AIModeSignInPromoControllerBase::MaybeShowPromo;

  // Triggers the sign-in promo bubble anchored to the browser window.
  // Returns false if `browser_window_interface` is null, its `BrowserView`
  // cannot be resolved, or promo eligibility checks (`CanShowPromo`) fail.
  // Otherwise, shows the promo and returns true.
  bool MaybeShowPromo(BrowserWindowInterface* browser_window_interface);

  base::WeakPtr<ComposeboxDriveSignInPromoController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  bool CanShowPromo(Profile& profile) override;
  std::unique_ptr<AIModeSignInPromoViewBase> CreatePromoView(
      views::BubbleAnchor anchor) override;

 private:
  base::WeakPtrFactory<ComposeboxDriveSignInPromoController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_CONTROLLER_H_
