// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_PRE_REDIRECTION_URL_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_PRE_REDIRECTION_URL_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;

}

namespace webapps {

// This class is responsible for observing a web content's navigations and
// storing the last URL that was accessed, not counting redirects (both
// HTTP-redirects via 300 codes, and javascript redirects), same-document
// navigations (e.g. #anchor tags), and subframe navigations. This is needed
// for applying the WebAppInstallForceList policy to manifests correctly,
// specifically custom names and icons for web apps.
class PreRedirectionURLObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PreRedirectionURLObserver> {
 public:
  PreRedirectionURLObserver(const PreRedirectionURLObserver&) = delete;
  PreRedirectionURLObserver& operator=(const PreRedirectionURLObserver&) =
      delete;
  // content::WebContentsObserver overrides
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  const GURL& last_url() const { return last_url_; }

 private:
  explicit PreRedirectionURLObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PreRedirectionURLObserver>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  GURL last_url_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_PRE_REDIRECTION_URL_OBSERVER_H_
