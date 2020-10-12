// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_TAB_STORAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_TAB_STORAGE_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace security_interstitials {

// A short-lived, per tab storage for mixed form interstitials, that stores a
// flag while proceeding, so a new interstitial is not shown immediately.
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

 private:
  explicit InsecureFormTabStorage(content::WebContents* contents) {}
  friend class content::WebContentsUserData<InsecureFormTabStorage>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Flag stores whether we are in the middle of a proceed action.
  bool is_proceeding_ = false;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_TAB_STORAGE_H_