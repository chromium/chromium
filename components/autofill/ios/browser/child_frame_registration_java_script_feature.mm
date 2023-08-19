// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/child_frame_registration_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/strings/string_number_conversions.h"
#import "base/unguessable_token.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

constexpr char kScriptName[] = "child_frame_registration";
constexpr char kScriptMessageName[] = "RegisterChildFrame";

// Local and remote frame IDs generated in JavaScript are equivalent to
// base::UnguessableToken (128 bits, cryptographically random).
absl::optional<base::UnguessableToken> DeserializeJavaScriptFrameId(
    const std::string& id) {
  // A valid ID is 128 bits, or 32 hex digits.
  if (id.length() != 32) {
    return {};
  }

  // Break string into first and last 16 hex digits.
  std::string high_hex = id.substr(0, 16);
  std::string low_hex = id.substr(16);

  uint64_t high, low;
  if (!base::HexStringToUInt64(high_hex, &high) ||
      !base::HexStringToUInt64(low_hex, &low)) {
    return {};
  }

  return base::UnguessableToken::Deserialize(high, low);
}

}  // namespace

namespace autofill {

// static
ChildFrameRegistrationJavaScriptFeature*
ChildFrameRegistrationJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ChildFrameRegistrationJavaScriptFeature> instance;
  return instance.get();
}

ChildFrameRegistrationJavaScriptFeature::
    ChildFrameRegistrationJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ChildFrameRegistrationJavaScriptFeature::
    ~ChildFrameRegistrationJavaScriptFeature() = default;

void ChildFrameRegistrationJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  base::Value::Dict* response = script_message.body()->GetIfDict();
  if (!response) {
    return;
  }

  const std::string* local_frame_id = response->FindString("local_frame_id");
  const std::string* remote_frame_id = response->FindString("remote_frame_id");
  if (!local_frame_id || !remote_frame_id) {
    return;
  }

  absl::optional<base::UnguessableToken> local =
      DeserializeJavaScriptFrameId(*local_frame_id);
  absl::optional<base::UnguessableToken> remote =
      DeserializeJavaScriptFrameId(*remote_frame_id);

  if (!local || !remote) {
    return;
  }

  // TODO(crbug.com/1440471): Handle double registration
  lookup_map.emplace(*remote, *local);
}

absl::optional<std::string>
ChildFrameRegistrationJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

}  // namespace autofill
