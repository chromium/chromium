// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/service/local_data_description.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class BubbleSignInPromoSignInButtonView;
class BubbleSignInPromoDelegate;

namespace content {
class WebContents;
}

// A personalized sign in promo used when Desktop Identity Consistency is
// enabled. Its display a message informing the user the benefits of signing in
// and a button that allows the user to sign in. The button has 3 different
// displays:
// * If Chrome has no accounts, then the promo button is a MD button allowing
//   the user to sign in to Chrome.
// * If Chrome has at least one account in web, then the promo button is
// personalized with the user full name and avatar icon and allows the user to
// sign in to Chrome.
// * If Chrome is in sign in pending state, then the promo is personalized with
// the user full name and avatar icon and allows the user to reauth.
class BubbleSignInPromoView : public views::View, public views::WidgetObserver {
  METADATA_HEADER(BubbleSignInPromoView, views::View)

 public:
  // Creates a personalized sign in promo view.
  // `button_style` is used to style non-personalized signin button. Otherwise,
  // the button remains prominent.
  BubbleSignInPromoView(
      content::WebContents* web_contents,
      signin_metrics::AccessPoint access_point,
      syncer::LocalDataItemModel::DataId data_id,
      ui::ButtonStyle button_style = ui::ButtonStyle::kProminent);
  BubbleSignInPromoView(const BubbleSignInPromoView&) = delete;
  BubbleSignInPromoView& operator=(const BubbleSignInPromoView&) = delete;
  ~BubbleSignInPromoView() override;

  View* GetSignInButton() const;

 private:
  // Used to sign in in when `signin_button_view_` is pressed.
  void SignIn();

  // views::WidgetObserver:
  // Records that an autofill sign in promo bubble has been dismissed.
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::View:
  void AddedToWidget() override;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};
  const signin_metrics::AccessPoint access_point_;
  raw_ptr<BubbleSignInPromoSignInButtonView> signin_button_view_ = nullptr;

  // Delegate to handle clicks on the sign in button.
  std::unique_ptr<BubbleSignInPromoDelegate> delegate_;
};
#endif  // CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_VIEW_H_
