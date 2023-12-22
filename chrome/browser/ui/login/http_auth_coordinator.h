// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_HTTP_AUTH_COORDINATOR_H_
#define CHROME_BROWSER_UI_LOGIN_HTTP_AUTH_COORDINATOR_H_

#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "net/base/auth.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// HTTP authentication is a feature that gates network resources behind a
// plaintext username/password ACL. This is typically implemented by
// web-browsers by showing a dialog to users part-way through a navigation with
// username/password textfields.
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication
//
// content::LoginDelegate is the API from //content that embedders use to
// implement Http authentication. This class is responsible for coordinating the
// browser side implementation fo content::LoginDelegate.
//
// TODO(https://crbug.com/1371177): Fix the flow to match below description.
// The control flow is the following:
//   (1) Extensions are given the opportunity to automatically fill in or
//       cancel http auth. If this happens, skip the remaining steps.
//   (2) If the network request is from a context that has no UI, then a dialog
//       cannot be shown. Cancel http auth.
//   (3a) If the network request is for a subframe, show the dialog.
//   (3b) If the network request is for the main frame, immediately cancel http
//        auth. Cancel the navigation and replace with a blank html page. Then
//        show the dialog.
//   (4) Once the dialog is shown, autofill is given the opportunity to
//       populate the fields.
//   (5) User clicks 'sign in'. Record data back into autofill.
//   (5a) Continuation from (3a). Sends callback via content::LoginDelegate.
//   (5b) Continuation from (3b). Reload the page.
//
// HttpAuthCoordinator is the class instantiated by ChromeContentBrowserClient
// that coordinates the flows. It exists for two reasons:
//   (1) Allow tests to perform dependency injection.
//   (2) A single place that owns all instances of HttpAuthFlow.
//
// One instance of HttpAuthFlow is created per call to CreateLoginDelegate().
// Each instance of HttpAuthFlow creates an instance of content::LoginDelegate
// which is returned and owned by the caller of CreateLoginDelegate().
// HttpAuthFlow can outlive this instance due to (3b) and (5b).
class HttpAuthCoordinator {
 public:
  HttpAuthCoordinator();
  virtual ~HttpAuthCoordinator();

  // Creates an instance of HttpAuthFlow.
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      content::WebContents* web_contents,
      const net::AuthChallengeInfo& auth_info,
      const content::GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback);
};

#endif  // CHROME_BROWSER_UI_LOGIN_HTTP_AUTH_COORDINATOR_H_
