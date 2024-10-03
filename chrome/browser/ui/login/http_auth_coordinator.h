// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_HTTP_AUTH_COORDINATOR_H_
#define CHROME_BROWSER_UI_LOGIN_HTTP_AUTH_COORDINATOR_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "net/base/auth.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
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
// TODO(crbug.com/40870289): Fix the flow to match below description.
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
//   (2) A single place that owns all instances of Flow.
//
// One instance of Flow is created per call to CreateLoginDelegate().
class HttpAuthCoordinator {
 public:
  HttpAuthCoordinator();
  virtual ~HttpAuthCoordinator();

  // Creates an instance of Flow.
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const net::AuthChallengeInfo& auth_info,
      const content::GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      bool is_request_for_navigation,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback);

  // Exposed for testing.
  virtual void CreateLoginTabHelper(content::WebContents* web_contents);

  // Exposed for testing.
  virtual std::unique_ptr<content::LoginDelegate>
  CreateLoginDelegateFromLoginHandler(
      content::WebContents* web_contents,
      const net::AuthChallengeInfo& auth_info,
      const content::GlobalRequestID& request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback);

 private:
  // See outer class comment for details.
  class Flow {
   public:
    Flow(HttpAuthCoordinator* coordinator,
         content::WebContents* web_contents,
         const net::AuthChallengeInfo& auth_info,
         const content::GlobalRequestID& request_id,
         bool is_request_for_primary_main_frame,
         bool is_request_for_navigation,
         const GURL& url,
         scoped_refptr<net::HttpResponseHeaders> response_headers,
         content::LoginDelegate::LoginAuthRequiredCallback
             auth_required_callback);
    ~Flow();
    Flow(const Flow&) = delete;
    Flow& operator=(const Flow&) = delete;

    // The wrapper is owned by //content. Usually destruction of the wrapper
    // will result in destruction of the Flow. However, the Flow will persist in
    // (3b) and (5b). See HttpAuthCoordinator class comment.
    void WrapperDestroyed();

    // Gives the extension subsystem the chance to respond to http auth. Returns
    // true if the extension subsystem is responding.
    bool ForwardToExtension(content::BrowserContext* browser_context);

    // Show a dialog to the user.
    void ShowDialog();

    // Returns a weak pointer to use with async callbacks.
    base::WeakPtr<Flow> GetWeakPtr();

   private:
    // Called by the extension subsystem with a response from the extension.
    void OnExtensionResponse(
        const std::optional<net::AuthCredentials>& credentials,
        bool should_cancel);

    // Called by LoginHandler when credentials are obtained or cancelled.
    void OnCredentials(const std::optional<net::AuthCredentials>& credentials);

    // The previous implementation of HttpAuth that is being refactored.
    std::unique_ptr<content::LoginDelegate> login_handler_;

    // Owns this instance.
    const raw_ptr<HttpAuthCoordinator> coordinator_;

    // Stores information about the original CreateLoginDelegate request.
    base::WeakPtr<content::WebContents> web_contents_;
    const net::AuthChallengeInfo auth_info_;
    const content::GlobalRequestID request_id_;
    const bool is_request_for_primary_main_frame_;
    const bool is_request_for_navigation_;
    const GURL url_;
    const scoped_refptr<net::HttpResponseHeaders> response_headers_;

    // Set to true if the extension cancels the request.
    bool did_cancel_from_extension_ = false;

    // Invoking this callback will destroy the wrapper. The one instance this
    // callback should not be invoked is if the wrapper is already destroyed.
    content::LoginDelegate::LoginAuthRequiredCallback callback_;

    base::WeakPtrFactory<Flow> weak_factory_{this};
  };

  // This is a dummy object returned to the caller of CreateLoginDelegate().
  class LoginDelegateWrapper : public content::LoginDelegate {
   public:
    explicit LoginDelegateWrapper(Flow* flow);
    ~LoginDelegateWrapper() override;
    LoginDelegateWrapper(const LoginDelegateWrapper&) = delete;
    LoginDelegateWrapper& operator=(const LoginDelegateWrapper&) = delete;

   private:
    raw_ptr<Flow> flow_;
  };

  // Called by a flow when it is finished.
  void FlowFinished(Flow* flow);

  // This member tracks all active flows.
  using Flows = std::map<Flow*, std::unique_ptr<Flow>>;
  Flows flows_;
};

#endif  // CHROME_BROWSER_UI_LOGIN_HTTP_AUTH_COORDINATOR_H_
