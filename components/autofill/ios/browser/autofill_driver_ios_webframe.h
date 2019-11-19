// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBFRAME_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBFRAME_H_

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
  // Creates a AutofillDriverIOSWebFrameFactory that will store all the
  // needed to create a AutofillDriverIOS.
  static void CreateForWebStateAndDelegate(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
  ~AutofillDriverIOSWebFrameFactory() override;

  AutofillDriverIOSWebFrameFactory(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);

  // Returns a AutofillDriverIOSFromWebFrame for |web_frame|, creating it if
  // needed.
  AutofillDriverIOSWebFrame* AutofillDriverIOSFromWebFrame(
      web::WebFrame* web_frame);

 private:
  friend class web::WebStateUserData<AutofillDriverIOSWebFrameFactory>;

  web::WebState* web_state_ = nullptr;
  AutofillClient* client_ = nullptr;
  id<AutofillDriverIOSBridge> bridge_ = nil;
  std::string app_locale_;
  AutofillManager::AutofillDownloadManagerState enable_download_manager_;
  WEB_STATE_USER_DATA_KEY_DECL();
};

// AutofillDriverIOSWebFrame will keep a refcountable AutofillDriverIOS. This is
// a workaround crbug.com/892612. On submission, AutofillDownloadManager and
// CreditCardSaveManager expect autofillManager and autofillDriver to live after
// web frame deletion so AutofillAgent will keep the latest submitted
// AutofillDriver alive.
// TODO(crbug.com/892612): remove this workaround once life cycle of autofill
// manager is fixed.
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
      AutofillManager::AutofillDownloadManagerState enable_download_manager);

 private:
  friend class base::RefCountedThreadSafe<AutofillDriverIOSRefCountable>;
  ~AutofillDriverIOSRefCountable() override = default;
};

// TODO(crbug.com/883203): Merge with AutofillDriverIOS class once WebFrame is
// released.
class AutofillDriverIOSWebFrame
    : public web::WebFrameUserData<AutofillDriverIOSWebFrame> {
 public:
  // Creates a AutofillDriverIOSWebFrame for |web_frame|.
  static void CreateForWebFrameAndDelegate(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);

  ~AutofillDriverIOSWebFrame() override;

  AutofillDriverIOS* driver() { return driver_.get(); }
  scoped_refptr<AutofillDriverIOSRefCountable> GetRetainableDriver();

  AutofillDriverIOSWebFrame(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
  scoped_refptr<AutofillDriverIOSRefCountable> driver_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_DRIVER_IOS_WEBSTATE_H_
