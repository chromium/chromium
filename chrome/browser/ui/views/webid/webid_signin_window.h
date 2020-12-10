// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_SIGNIN_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_SIGNIN_WINDOW_H_

#include <memory>
#include <string>

#include "base/callback.h"

class GURL;

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

// The WebIDSigninWindow loads Idp sign-in page in a modal allowing user to
// sign in. The modal may be closed by user or once Idp sign-in page has
// completed its process and have called the appropriate JS callback.
class WebIDSigninWindow {
 public:
  // Calls the  provided callback when IDP has provided an id_token with the
  // id_token a its argument, or when window is closed by user with an empty
  // string as its argument.
  WebIDSigninWindow(content::WebContents* initiator_web_contents,
                    content::WebContents* idp_web_contents,
                    const GURL& provider,
                    base::OnceCallback<void()> on_done);
  WebIDSigninWindow(const WebIDSigninWindow&) = delete;
  WebIDSigninWindow& operator=(const WebIDSigninWindow&) = delete;

  void Close();

 private:
  // This class manages its own lifetime which is controlled by the view
  // hierarchy. Once modal is deleted, this gets deleted as well.
  ~WebIDSigninWindow();

  views::Widget* modal_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_SIGNIN_WINDOW_H_
