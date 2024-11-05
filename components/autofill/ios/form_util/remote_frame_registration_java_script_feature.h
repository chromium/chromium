// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_REMOTE_FRAME_REGISTRATION_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_REMOTE_FRAME_REGISTRATION_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace autofill {

// Name of the message handler registered by
// RemoteFrameRegistrationJavaScriptFeature.
inline constexpr char kRemoteFrameRegistrationMessageHandlerName[] =
    "FrameRegistrationMessage";
// Name of the script injected by RemoteFrameRegistrationJavaScriptFeature.
inline constexpr char kRemoteFrameRegistrationScriptName[] =
    "child_frame_registration_lib";

// Injects a script with functions for registering frames with
// RemoteFrameToken's.
class RemoteFrameRegistrationJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static RemoteFrameRegistrationJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<RemoteFrameRegistrationJavaScriptFeature>;
  // TODO(crbug.com/359538514): Remove friend once isolated world for Autofill
  // is launched.
  friend class TestAutofillJavaScriptFeatureContainer;

  // web::JavaScriptFeature
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  RemoteFrameRegistrationJavaScriptFeature();
  ~RemoteFrameRegistrationJavaScriptFeature() override;

  RemoteFrameRegistrationJavaScriptFeature(
      const RemoteFrameRegistrationJavaScriptFeature&) = delete;
  RemoteFrameRegistrationJavaScriptFeature& operator=(
      const RemoteFrameRegistrationJavaScriptFeature&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_REMOTE_FRAME_REGISTRATION_JAVA_SCRIPT_FEATURE_H_
