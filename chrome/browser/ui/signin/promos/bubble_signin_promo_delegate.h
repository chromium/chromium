// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/service/local_data_description.h"

struct AccountInfo;

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace content {
class WebContents;
}  // namespace content

// Delegate for the bubble sign in promo view.
class BubbleSignInPromoDelegate {
 public:
  BubbleSignInPromoDelegate(content::WebContents& web_contents,
                            signin_metrics::AccessPoint access_point,
                            syncer::LocalDataItemModel::DataId data_id);

  BubbleSignInPromoDelegate(BubbleSignInPromoDelegate&) = delete;
  BubbleSignInPromoDelegate& operator=(const BubbleSignInPromoDelegate&) =
      delete;

  virtual ~BubbleSignInPromoDelegate();

  // Informs the delegate to sign in `account` or to present
  // the browser sign-in page if `account` is empty.
  virtual void OnSignIn(const AccountInfo& account);

  content::WebContents* GetWebContents() { return web_contents_.get(); }

 private:
  // Used to move the local data item to the account storage once the sign in
  // has been completed.
  const syncer::LocalDataItemModel::DataId data_id_;
  base::WeakPtr<content::WebContents> web_contents_;
  signin_metrics::AccessPoint access_point_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_DELEGATE_H_
