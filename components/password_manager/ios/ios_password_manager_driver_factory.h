// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#include "components/password_manager/ios/password_manager_driver_bridge.h"
#include "ios/web/public/js_messaging/web_frame_user_data.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

class IOSPasswordManagerDriverFactory
    : public web::WebStateUserData<IOSPasswordManagerDriverFactory> {
 public:
  // This method is used to retrieve the correspondent driver of a web frame.
  static IOSPasswordManagerDriver* FromWebStateAndWebFrame(
      web::WebState* web_state,
      web::WebFrame* web_frame);

  IOSPasswordManagerDriverFactory(const IOSPasswordManagerDriverFactory&) =
      delete;
  IOSPasswordManagerDriverFactory& operator=(
      const IOSPasswordManagerDriverFactory&) = delete;

  ~IOSPasswordManagerDriverFactory() override;

  // Gets a retainable driver object which will still exist after the frame is
  // destroyed.
  static scoped_refptr<IOSPasswordManagerDriver> GetRetainableDriver(
      web::WebState* web_state,
      web::WebFrame* web_frame);

 private:
  friend class web::WebStateUserData<IOSPasswordManagerDriverFactory>;

  // To create/get a new driver, use
  // IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame.
  // This method creates/gets a new IOSPasswordManagerWebFrameDriverHelper
  // and returns the IOSPasswordManagerDriver associated to it.
  IOSPasswordManagerDriver* IOSPasswordManagerDriver(web::WebFrame* web_frame,
                                                     web::WebState* web_state);

  // To create a factory, use the
  // IOSPasswordManagerDriverFactory::CreateForWebState method.
  IOSPasswordManagerDriverFactory(
      web::WebState* web_state,
      id<PasswordManagerDriverBridge> bridge,
      password_manager::PasswordManagerInterface* password_manager);

  id<PasswordManagerDriverBridge> bridge_;
  raw_ptr<password_manager::PasswordManagerInterface> password_manager_;
  int next_free_id = 0;

  WEB_STATE_USER_DATA_KEY_DECL();
};

// This class is tied to the web frame and owns a reference to
// a IOSPasswordManagerDriver ref countable object.
class IOSPasswordManagerWebFrameDriverHelper
    : public web::WebFrameUserData<IOSPasswordManagerWebFrameDriverHelper> {
 public:
  IOSPasswordManagerWebFrameDriverHelper(
      const IOSPasswordManagerWebFrameDriverHelper&) = delete;
  IOSPasswordManagerWebFrameDriverHelper& operator=(
      const IOSPasswordManagerWebFrameDriverHelper&) = delete;

  ~IOSPasswordManagerWebFrameDriverHelper() override;

 private:
  // All of the methods are private so that no one uses them while trying to
  // create/get a driver. However, IOSPasswordManagerDriverFactory needs to be
  // able to access them in the driver creation/retrieval flow.
  friend class IOSPasswordManagerDriverFactory;

  // To create a new driver, use
  // IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame.
  // Creates a IOSPasswordManagerWebFrameDriverHelper object which is tied to
  // the WebFrame using WebFrameUserData::SetUserData.
  static void CreateForWebFrame(
      web::WebState* web_state,
      id<PasswordManagerDriverBridge> bridge,
      password_manager::PasswordManagerInterface* password_manager,
      web::WebFrame* web_frame,
      int driver_id);

  // The constructor creates a ref countable IOSPasswordManagerDriver and saves
  // it in the driver_ field.
  IOSPasswordManagerWebFrameDriverHelper(
      web::WebState* web_state,
      id<PasswordManagerDriverBridge> bridge,
      password_manager::PasswordManagerInterface* password_manager,
      web::WebFrame* web_frame,
      int driver_id);

  IOSPasswordManagerDriver* driver() { return driver_.get(); }
  scoped_refptr<IOSPasswordManagerDriver> RetainableDriver() { return driver_; }

  // driver_ is a ref countable IOSPasswordManagerDriver. On submission,
  // PasswordManager expects SharedPasswordController and
  // IOSPasswordManagerDriver to live after web frame deletion so
  // SharedPasswordController will keep the latest submitted
  // IOSPasswordManagerDriver alive.
  scoped_refptr<IOSPasswordManagerDriver> driver_;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_FACTORY_H_
