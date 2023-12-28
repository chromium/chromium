// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/child_frame_registrar.h"

#import "base/strings/string_util.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

class ChildFrameRegistrarTest : public PlatformTest {
 protected:
  ChildFrameRegistrarTest() {
    ChildFrameRegistrar::CreateForWebState(&web_state_);
  }

  ChildFrameRegistrar* registrar() {
    return ChildFrameRegistrar::FromWebState(&web_state_);
  }

  web::FakeWebState web_state_;
};

// Happy path: pass a valid remote/local token pair and confirm that we can get
// back the local token on request.
TEST_F(ChildFrameRegistrarTest, RegisterAndLookup) {
  const std::string local_frame_id = std::string(32, 'a');
  const std::string remote_frame_id = std::string(32, 'b');

  autofill::LocalFrameToken local_token(
      *DeserializeJavaScriptFrameId(local_frame_id));
  autofill::RemoteFrameToken remote_token(
      *DeserializeJavaScriptFrameId(remote_frame_id));

  base::Value::Dict dict;
  dict.Set("local_frame_id", local_frame_id);
  dict.Set("remote_frame_id", remote_frame_id);
  base::Value dict_as_value(std::move(dict));
  registrar()->ProcessRegistrationMessage(&dict_as_value);

  auto lookup_result = registrar()->LookupChildFrame(remote_token);
  ASSERT_TRUE(lookup_result.has_value());
  EXPECT_EQ(*lookup_result, local_token);
}

// Test that an invalid frame token is not accepted by the registrar.
TEST_F(ChildFrameRegistrarTest, JunkDoesntRegister) {
  // Use a valid remote token so that we can actually perform a lookup later.
  const std::string valid_remote_id = std::string(32, '1');
  autofill::RemoteFrameToken valid_remote_token(
      *autofill::DeserializeJavaScriptFrameId(valid_remote_id));

  base::Value::Dict dict;
  dict.Set("local_frame_id", "this is junk");
  dict.Set("remote_frame_id", valid_remote_id);
  base::Value dict_as_value(std::move(dict));
  registrar()->ProcessRegistrationMessage(&dict_as_value);

  auto lookup_result = registrar()->LookupChildFrame(valid_remote_token);
  EXPECT_FALSE(lookup_result.has_value());

  // Also run with a non-dict Value, just to make sure it doesn't crash.
  base::Value junk(7);
  registrar()->ProcessRegistrationMessage(&junk);
}

}  // namespace autofill
