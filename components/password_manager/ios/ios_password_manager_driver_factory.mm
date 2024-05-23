// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver_factory.h"

#include "components/password_manager/core/browser/password_manager.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"

// static
IOSPasswordManagerDriver*
IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  return !web_frame ? nullptr
                    : IOSPasswordManagerDriverFactory::FromWebState(web_state)
                          ->IOSPasswordManagerDriver(web_frame, web_state);
}

IOSPasswordManagerDriver*
IOSPasswordManagerDriverFactory::IOSPasswordManagerDriver(
    web::WebFrame* web_frame,
    web::WebState* web_state) {
  IOSPasswordManagerWebFrameDriverHelper::CreateForWebFrame(
      web_state, bridge_, password_manager_, web_frame, next_free_id++);
  return !web_frame
             ? nullptr
             : IOSPasswordManagerWebFrameDriverHelper::FromWebFrame(web_frame)
                   ->driver();
}

IOSPasswordManagerDriverFactory::IOSPasswordManagerDriverFactory(
    web::WebState* web_state,
    id<PasswordManagerDriverBridge> bridge,
    password_manager::PasswordManagerInterface* password_manager)
    : bridge_(bridge), password_manager_(password_manager) {}

IOSPasswordManagerDriverFactory::~IOSPasswordManagerDriverFactory() = default;

// static
scoped_refptr<IOSPasswordManagerDriver>
IOSPasswordManagerDriverFactory::GetRetainableDriver(web::WebState* web_state,
                                                     web::WebFrame* web_frame) {
  if (!IOSPasswordManagerWebFrameDriverHelper::FromWebFrame(web_frame)) {
    IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(web_state,
                                                             web_frame);
  }
  return IOSPasswordManagerWebFrameDriverHelper::FromWebFrame(web_frame)
      ->RetainableDriver();
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSPasswordManagerDriverFactory)

// static
void IOSPasswordManagerWebFrameDriverHelper::CreateForWebFrame(
    web::WebState* web_state,
    id<PasswordManagerDriverBridge> bridge,
    password_manager::PasswordManagerInterface* password_manager,
    web::WebFrame* web_frame,
    int driver_id) {
  if (!web_frame || FromWebFrame(web_frame) || !web_state) {
    return;
  }

  web_frame->SetUserData(
      UserDataKey(),
      absl::WrapUnique(new IOSPasswordManagerWebFrameDriverHelper(
          web_state, bridge, password_manager, web_frame, driver_id)));
}

IOSPasswordManagerWebFrameDriverHelper::IOSPasswordManagerWebFrameDriverHelper(
    web::WebState* web_state,
    id<PasswordManagerDriverBridge> bridge,
    password_manager::PasswordManagerInterface* password_manager,
    web::WebFrame* web_frame,
    int driver_id)
    : driver_(
          base::WrapRefCounted(new IOSPasswordManagerDriver(web_state,
                                                            bridge,
                                                            password_manager,
                                                            web_frame,
                                                            driver_id))) {}

IOSPasswordManagerWebFrameDriverHelper::
    ~IOSPasswordManagerWebFrameDriverHelper() = default;
