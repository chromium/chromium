// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBFRAME_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBFRAME_H_

#include "base/types/pass_key.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "ios/web/public/js_messaging/web_frame_user_data.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

class AutofillDriverIOSWebFrame;

// This factory will keep the parameters needed to create an
// AutofillDriverIOSWebFrame. These parameters only depend on the web_state, so
// there is one AutofillDriverIOSWebFrameFactory per WebState.
class AutofillDriverIOSWebFrameFactory
    : public web::WebStateUserData<AutofillDriverIOSWebFrameFactory> {
 public:
  ~AutofillDriverIOSWebFrameFactory() override;

  // Returns a AutofillDriverIOSFromWebFrame for |web_frame|, creating it if
  // needed.
  AutofillDriverIOSWebFrame* AutofillDriverIOSFromWebFrame(
      web::WebFrame* web_frame);

 private:
  friend class web::WebStateUserData<AutofillDriverIOSWebFrameFactory>;

  // Creates a AutofillDriverIOSWebFrameFactory that will store all the
  // needed to create a AutofillDriverIOS.
  AutofillDriverIOSWebFrameFactory(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::EnableDownloadManager enable_download_manager);

  web::WebState* web_state_ = nullptr;
  AutofillClient* client_ = nullptr;
  id<AutofillDriverIOSBridge> bridge_ = nil;
  std::string app_locale_;
  AutofillManager::EnableDownloadManager enable_download_manager_;
  WEB_STATE_USER_DATA_KEY_DECL();
};

// AutofillDriverIOSWebFrame keeps a refcountable AutofillDriverIOS. See its
// documentation for details.
class AutofillDriverIOSRefCountable
    : public AutofillDriverIOS,
      public base::RefCountedThreadSafe<AutofillDriverIOSRefCountable> {
 public:
  AutofillDriverIOSRefCountable(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::EnableDownloadManager enable_download_manager);

 private:
  friend class base::RefCountedThreadSafe<AutofillDriverIOSRefCountable>;
  ~AutofillDriverIOSRefCountable() override = default;
};

// Wraps a ref-counted AutofillDriverIOS. This allows AutofillAgent to extend
// the lifetime of AutofillDriverIOS beyond the lifetime of the associated
// WebFrame up until the destruction of the WebState.
//
// This lifetime extension is a workaround for crbug.com/892612 to let the
// asynchronous task in AutofillDownloadManager (which is owned by
// BrowserAutofillManager, which is owned by AutofillDriverIOS) finish.
//
// TODO(crbug.com/892612, crbug.com/1394786): Remove this workaround once life
// cycle of AutofillDownloadManager is fixed.
class AutofillDriverIOSWebFrame
    : public web::WebFrameUserData<AutofillDriverIOSWebFrame> {
 public:
  static void CreateForWebFrame(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::EnableDownloadManager enable_download_manager);

  ~AutofillDriverIOSWebFrame() override;

  AutofillDriverIOS* driver() { return driver_.get(); }

  // AutofillAgent calls this function to extend the AutofillDriverIOS's
  // lifetime until the associated WebState (not WebFrame) is destroyed.
  //
  // It does so by keeping a copy of the refcounted driver pointer in
  // AutofillAgent::_last_submitted_autofill_driver and resetting that pointer
  // pointer in webStateDestroyed().
  scoped_refptr<AutofillDriverIOSRefCountable> GetRetainableDriver() {
    return driver_;
  }

 private:
  AutofillDriverIOSWebFrame(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::EnableDownloadManager enable_download_manager);

  scoped_refptr<AutofillDriverIOSRefCountable> driver_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBFRAME_H_
