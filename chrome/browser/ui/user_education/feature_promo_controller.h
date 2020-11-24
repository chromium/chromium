// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace base {
struct Feature;
}

class FeaturePromoTextReplacements;

// Manages display of in-product help promos. All IPH displays in Top
// Chrome should go through here.
class FeaturePromoController {
 public:
  FeaturePromoController() = default;
  virtual ~FeaturePromoController() = default;

  using BubbleCloseCallback = base::OnceCallback<void()>;

  // Starts the promo if possible. Returns whether it started.
  // |iph_feature| must be an IPH feature defined in
  // components/feature_engagement/public/feature_list.cc and registered
  // with |FeaturePromoRegistry|. Note that this is different than the
  // feature that the IPH is showing for.
  //
  // If a bubble was shown and |close_callback| was provided, it will be
  // called when the bubble closes. |close_callback| must be valid as
  // long as the bubble shows.
  //
  // For users that can't register their parameters with
  // FeaturePromoRegistry, see
  // |FeaturePromoControllerViews::MaybeShowPromoWithParams()|. Prefer
  // statically registering params with FeaturePromoRegistry and using
  // this method when possible.
  virtual bool MaybeShowPromo(
      const base::Feature& iph_feature,
      BubbleCloseCallback close_callback = BubbleCloseCallback()) = 0;

  // Like the above, but adds context-specific text in the promo
  // bubble's body text. The correct usage of |text_replacements|
  // depends on how the promo is registered with the implementation. It
  // should have one replacement for each placeholder in the registered
  // body text.
  virtual bool MaybeShowPromoWithTextReplacements(
      const base::Feature& iph_feature,
      FeaturePromoTextReplacements text_replacements,
      BubbleCloseCallback close_callback = BubbleCloseCallback()) = 0;

  // Returns whether a bubble is showing for the given IPH. Note that if
  // this is false, a promo might still be in progress; for example, a
  // promo may have continued into a menu in which case the bubble is no
  // longer showing.
  virtual bool BubbleIsShowing(const base::Feature& iph_feature) const = 0;

  // If a bubble is showing for |iph_feature| close it and end the
  // promo. Does nothing otherwise. Returns true if a bubble was closed
  // and false otherwise.
  //
  // Calling this has no effect if |CloseBubbleAndContinuePromo()| was
  // called for |iph_feature|.
  virtual bool CloseBubble(const base::Feature& iph_feature) = 0;

  class PromoHandle;

  // Like CloseBubble() but does not end the promo yet. The caller takes
  // ownership of the promo (e.g. to show a highlight in a menu or on a
  // button). The returned PromoHandle represents this ownership.
  virtual PromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) = 0;

  // When a caller wants to take ownership of the promo after a bubble
  // is closed, this handle is given. It must be dropped in a timely
  // fashion to ensure everything is cleaned up. If it isn't, it will
  // make the IPH backend think it's still shwoing and block all other
  // IPH indefinitely.
  class PromoHandle {
   public:
    explicit PromoHandle(base::WeakPtr<FeaturePromoController> controller);
    PromoHandle(PromoHandle&&);
    ~PromoHandle();

    PromoHandle& operator=(PromoHandle&&);

   private:
    base::WeakPtr<FeaturePromoController> controller_;
  };

 protected:
  // Called when PromoHandle is destroyed to finish the promo.
  virtual void FinishContinuedPromo() = 0;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_H_
