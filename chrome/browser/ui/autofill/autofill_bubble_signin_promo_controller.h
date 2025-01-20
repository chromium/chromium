// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/service/local_data_description.h"

struct AccountInfo;
namespace signin {
enum class SignInAutofillBubblePromoType;
}  // namespace signin

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// This controller provides data and actions for the
// AutofillSignInPromoBubbleView.
class AutofillBubbleSignInPromoController {
 public:
  explicit AutofillBubbleSignInPromoController(
      content::WebContents& web_contents,
      signin_metrics::AccessPoint access_point,
      syncer::LocalDataItemModel::DataId data_id);
  ~AutofillBubbleSignInPromoController();

  // Called by the view when the "Sign in" button in the promo bubble is
  // clicked.
  void OnSignInToChromeClicked(const AccountInfo& account);

  content::WebContents* GetWebContents() { return web_contents_.get(); }

 private:
  // Used to move the local data item to the account storage once the sign in
  // has been completed.
  const syncer::LocalDataItemModel::DataId data_id_;
  base::WeakPtr<content::WebContents> web_contents_;
  signin_metrics::AccessPoint access_point_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_
