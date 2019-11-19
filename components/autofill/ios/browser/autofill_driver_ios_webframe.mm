// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"

namespace autofill {

// static
void AutofillDriverIOSWebFrameFactory::CreateForWebStateAndDelegate(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(
      UserDataKey(),
      std::make_unique<AutofillDriverIOSWebFrameFactory>(
          web_state, client, bridge, app_locale, enable_download_manager));
}

AutofillDriverIOSWebFrameFactory::AutofillDriverIOSWebFrameFactory(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : web_state_(web_state),
      client_(client),
      bridge_(bridge),
      app_locale_(app_locale),
      enable_download_manager_(enable_download_manager) {}

AutofillDriverIOSWebFrameFactory::~AutofillDriverIOSWebFrameFactory() {}

AutofillDriverIOSWebFrame*
AutofillDriverIOSWebFrameFactory::AutofillDriverIOSFromWebFrame(
    web::WebFrame* web_frame) {
  AutofillDriverIOSWebFrame::CreateForWebFrameAndDelegate(
      web_state_, web_frame, client_, bridge_, app_locale_,
      enable_download_manager_);
  return AutofillDriverIOSWebFrame::FromWebFrame(web_frame);
}

// static
void AutofillDriverIOSWebFrame::CreateForWebFrameAndDelegate(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebFrame(web_frame))
    return;

  web_frame->SetUserData(UserDataKey(),
                         std::make_unique<AutofillDriverIOSWebFrame>(
                             web_state, web_frame, client, bridge, app_locale,
                             enable_download_manager));
}

AutofillDriverIOSRefCountable::AutofillDriverIOSRefCountable(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)

    : AutofillDriverIOS(web_state,
                        web_frame,
                        client,
                        bridge,
                        app_locale,
                        enable_download_manager) {}

AutofillDriverIOSWebFrame::AutofillDriverIOSWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : driver_(base::MakeRefCounted<AutofillDriverIOSRefCountable>(
          web_state,
          web_frame,
          client,
          bridge,
          app_locale,
          enable_download_manager)) {}

AutofillDriverIOSWebFrame::~AutofillDriverIOSWebFrame() {}

scoped_refptr<AutofillDriverIOSRefCountable>
AutofillDriverIOSWebFrame::GetRetainableDriver() {
  return driver_;
}

WEB_STATE_USER_DATA_KEY_IMPL(AutofillDriverIOSWebFrameFactory)

}  //  namespace autofill
