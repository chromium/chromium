// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"

namespace autofill {

AutofillDriverIOSWebFrameFactory::AutofillDriverIOSWebFrameFactory(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale)
    : web_state_(web_state),
      client_(client),
      bridge_(bridge),
      app_locale_(app_locale) {}

AutofillDriverIOSWebFrameFactory::~AutofillDriverIOSWebFrameFactory() {}

AutofillDriverIOSWebFrame*
AutofillDriverIOSWebFrameFactory::AutofillDriverIOSFromWebFrame(
    web::WebFrame* web_frame) {
  AutofillDriverIOSWebFrame::CreateForWebFrame(web_state_, web_frame, client_,
                                               bridge_, app_locale_);
  return AutofillDriverIOSWebFrame::FromWebFrame(web_frame);
}

// static
void AutofillDriverIOSWebFrame::CreateForWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale) {
  if (FromWebFrame(web_frame))
    return;

  web_frame->SetUserData(
      UserDataKey(), base::WrapUnique(new AutofillDriverIOSWebFrame(
                         web_state, web_frame, client, bridge, app_locale)));
}

AutofillDriverIOSRefCountable::AutofillDriverIOSRefCountable(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale)
    : AutofillDriverIOS(web_state, web_frame, client, bridge, app_locale) {}

AutofillDriverIOSWebFrame::AutofillDriverIOSWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale)
    : driver_(base::MakeRefCounted<AutofillDriverIOSRefCountable>(web_state,
                                                                  web_frame,
                                                                  client,
                                                                  bridge,
                                                                  app_locale)) {
}

AutofillDriverIOSWebFrame::~AutofillDriverIOSWebFrame() {}

WEB_STATE_USER_DATA_KEY_IMPL(AutofillDriverIOSWebFrameFactory)

}  //  namespace autofill
