// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_CHILD_FRAME_REGISTRATION_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_CHILD_FRAME_REGISTRATION_JAVA_SCRIPT_FEATURE_H_

#import <map>

#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

namespace web {
class ScriptMessage;
class WebState;
}  // namespace web

namespace autofill {

// Child frame registration is the process whereby a frame can assign an ID (a
// remote frame token) to a child frame, establishing a relationship between
// that frame in the DOM (and JS) and the corresponding WebFrame object in C++.
// This class maintains those mappings.
class ChildFrameRegistrationJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static ChildFrameRegistrationJavaScriptFeature* GetInstance();
  ChildFrameRegistrationJavaScriptFeature();
  ~ChildFrameRegistrationJavaScriptFeature() override;

  // Maps from remote to local tokens for all registered frames, to allow
  // lookup of a frame based on its remote token.
  //
  // Frame Tokens are used by browser-layer Autofill code to identify and
  // interact with a specific frame. Local Frame Tokens must not leak to frames
  // other than the ones they identify, while Remote Frame Tokens are also known
  // to the parent frame.
  //
  // In the context of iOS, the LocalFrameToken is equal to the frameId and can
  // be used to fetch the appropriate WebFrame from the WebFramesManager.
  //
  // TODO(crbug.com/1440471): Encapsulate this with a public lookup method.
  std::map<RemoteFrameToken, LocalFrameToken> lookup_map;

 protected:
  // JavaScriptFeature:
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& script_message) override;
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_CHILD_FRAME_REGISTRATION_JAVA_SCRIPT_FEATURE_H_
