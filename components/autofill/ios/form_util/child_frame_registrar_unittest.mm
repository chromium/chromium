// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/child_frame_registrar.h"

#import "base/strings/string_util.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace autofill {

class ChildFrameRegistrarTest : public PlatformTest {
 protected:
  ChildFrameRegistrarTest() {
    auto page_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    page_frames_manager_ = page_frames_manager.get();
    web_state_.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                   std::move(page_frames_manager));

    auto isolated_frames_manager =
        std::make_unique<web::FakeWebFramesManager>();
    isolated_frames_manager_ = isolated_frames_manager.get();
    web_state_.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(isolated_frames_manager));

    ChildFrameRegistrar::CreateForWebState(&web_state_);
  }

  ChildFrameRegistrar* registrar() {
    return ChildFrameRegistrar::FromWebState(&web_state_);
  }

  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> page_frames_manager_ = nullptr;
  raw_ptr<web::FakeWebFramesManager> isolated_frames_manager_ = nullptr;
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

// Test that stale local tokens are cleaned up when frames are removed.
TEST_F(ChildFrameRegistrarTest, CleanUpStaleLocalFrameTokens) {
  // Add a frame to the page content world manager.
  const std::string local_frame_id1 = std::string(32, 'a');
  page_frames_manager_->AddWebFrame(
      web::FakeWebFrame::Create(local_frame_id1, /*is_main_frame=*/true,
                                /*security_origin=*/GURL()));

  autofill::LocalFrameToken local_token1(
      *DeserializeJavaScriptFrameId(local_frame_id1));

  // Map two remote tokens to the frame id.
  autofill::RemoteFrameToken remote_token1(
      *DeserializeJavaScriptFrameId(std::string(32, 'b')));
  registrar()->RegisterMapping(remote_token1, local_token1);

  autofill::RemoteFrameToken remote_token2(
      *DeserializeJavaScriptFrameId(std::string(32, 'c')));
  registrar()->RegisterMapping(remote_token2, local_token1);

  ASSERT_TRUE(registrar()->LookupChildFrame(remote_token1));
  ASSERT_TRUE(registrar()->LookupChildFrame(remote_token2));

  // Add another mapping that should not be affected by the frame deletion.
  autofill::LocalFrameToken local_token2(
      *DeserializeJavaScriptFrameId(std::string(32, 'd')));
  autofill::RemoteFrameToken remote_token3(
      *DeserializeJavaScriptFrameId(std::string(32, 'e')));
  registrar()->RegisterMapping(remote_token3, local_token2);

  // Delete the frame.
  page_frames_manager_->RemoveWebFrame(local_frame_id1);

  // Validate that the entries with `local_frame_id1` were cleaned up.
  ASSERT_FALSE(registrar()->LookupChildFrame(remote_token1));
  ASSERT_FALSE(registrar()->LookupChildFrame(remote_token2));

  // Validate that the other entry was not deleted.
  ASSERT_TRUE(registrar()->LookupChildFrame(remote_token3));

  // Test the same in the isolated world.
  isolated_frames_manager_->AddWebFrame(
      web::FakeWebFrame::Create(local_frame_id1, /*is_main_frame=*/true,
                                /*security_origin=*/GURL()));

  // Map two remote tokens to the frame id.
  registrar()->RegisterMapping(remote_token1, local_token1);
  registrar()->RegisterMapping(remote_token2, local_token1);

  ASSERT_TRUE(registrar()->LookupChildFrame(remote_token1));
  ASSERT_TRUE(registrar()->LookupChildFrame(remote_token2));

  // Delete the frame.
  isolated_frames_manager_->RemoveWebFrame(local_frame_id1);

  // Validate that the entries with `local_frame_id` were cleaned up.
  ASSERT_FALSE(registrar()->LookupChildFrame(remote_token1));
  ASSERT_FALSE(registrar()->LookupChildFrame(remote_token2));

  // Validate that the other entry was not deleted.
  ASSERT_TRUE(registrar()->LookupChildFrame(remote_token3));
}

}  // namespace autofill
