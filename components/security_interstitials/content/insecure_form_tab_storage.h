// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_TAB_STORAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_TAB_STORAGE_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace security_interstitials {

// A short-lived, per tab storage for mixed form interstitials, that:
// -Stores a flag while proceeding, so a new interstitial is not shown
//  immediately.
// -Stores a flag when an interstitial is currently being shown, to prevent
//  reloading the interstitial from causing it to be bypassed in certain cases
//  (e.g. for GET form submissions).
class InsecureFormTabStorage
    : public content::WebContentsUserData<InsecureFormTabStorage> {
 public:
  ~InsecureFormTabStorage() override;

  // Disallow copy and assign.
  InsecureFormTabStorage(const InsecureFormTabStorage&) = delete;
  InsecureFormTabStorage& operator=(const InsecureFormTabStorage&) = delete;

  // Returns the InsecureFormTabStorage associated to |web_contents|, or creates
  // one if there is none.
  static InsecureFormTabStorage* GetOrCreate(
      content::WebContents* web_contents);

  void SetIsProceeding(bool is_proceeding) { is_proceeding_ = is_proceeding; }
  bool IsProceeding() const { return is_proceeding_; }
  void SetInterstitialShown(bool interstitial_shown) {
    interstitial_shown_ = interstitial_shown;
  }
  bool InterstitialShown() const { return interstitial_shown_; }

 private:
  explicit InsecureFormTabStorage(content::WebContents* contents);
  friend class content::WebContentsUserData<InsecureFormTabStorage>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Flag stores whether we are in the middle of a proceed action.
  bool is_proceeding_ = false;
  // Flag stores whether an interstitial was shown for the current navigation.
  bool interstitial_shown_ = false;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_TAB_STORAGE_H_
