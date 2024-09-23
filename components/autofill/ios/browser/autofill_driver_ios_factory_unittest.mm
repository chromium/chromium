// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"

#import <memory>

#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/test_matchers.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory_test_api.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::autofill::test::LazyRef;
using ::autofill::test::SaveArgPtr;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Truly;

using LifecycleState = AutofillDriver::LifecycleState;
using enum LifecycleState;

auto HasState(LifecycleState expected_state) {
  return Property("AutofillDriver::GetLifecycleState",
                  &AutofillDriver::GetLifecycleState, expected_state);
}

class MockWebFramesManagerObserver : public web::WebFramesManager::Observer {
 public:
  MockWebFramesManagerObserver() = default;
  MockWebFramesManagerObserver(const MockWebFramesManagerObserver&) = delete;
  MockWebFramesManagerObserver& operator=(const MockWebFramesManagerObserver&) =
      delete;
  ~MockWebFramesManagerObserver() override = default;
  MOCK_METHOD(void,
              WebFrameBecameAvailable,
              (web::WebFramesManager * web_frames_manager,
               web::WebFrame* web_frame),
              (override));
  MOCK_METHOD(void,
              WebFrameBecameUnavailable,
              (web::WebFramesManager * web_frames_manager,
               const std::string& frame_id),
              (override));
};

class MockAutofillDriverIOSFactoryObserver
    : public AutofillDriverIOSFactory::Observer {
 public:
  MockAutofillDriverIOSFactoryObserver() = default;
  MockAutofillDriverIOSFactoryObserver(
      const MockAutofillDriverIOSFactoryObserver&) = delete;
  MockAutofillDriverIOSFactoryObserver& operator=(
      const MockAutofillDriverIOSFactoryObserver&) = delete;
  ~MockAutofillDriverIOSFactoryObserver() override = default;
  MOCK_METHOD(void,
              OnAutofillDriverIOSFactoryDestroyed,
              (AutofillDriverIOSFactory & factory),
              (override));
  MOCK_METHOD(void,
              OnAutofillDriverIOSCreated,
              (AutofillDriverIOSFactory & factory, AutofillDriverIOS& driver),
              (override));
  MOCK_METHOD(void,
              OnAutofillDriverIOSStateChanged,
              (AutofillDriverIOSFactory & factory,
               AutofillDriverIOS& driver,
               AutofillDriver::LifecycleState old_state,
               AutofillDriver::LifecycleState new_state),
              (override));
};

class AutofillDriverIOSFactoryTest : public web::WebTest {
 public:
  AutofillDriverIOSFactoryTest() = default;
  ~AutofillDriverIOSFactoryTest() override = default;

  void SetUp() override {
    web::WebTest::SetUp();

    OverrideJavaScriptFeatures({AutofillJavaScriptFeature::GetInstance()});

    web_state_.SetWebFramesManager(
        content_world(), std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetContentIsHTML(true);
    web_state_.SetBrowserState(GetBrowserState());
    web_frames_manager().AddObserver(&pre_factory_);
    ASSERT_FALSE(AutofillDriverIOSFactory::FromWebState(&web_state_));
    AutofillDriverIOSFactory::CreateForWebState(&web_state_, &client_, nil,
                                                "en-US");
    factory().AddObserver(&factory_observer_);
    web_frames_manager().AddObserver(&post_factory_);
  }

  void TearDown() override {
    web_frames_manager().RemoveObserver(&pre_factory_);
    factory().RemoveObserver(&factory_observer_);
    web_frames_manager().RemoveObserver(&post_factory_);
  }

  std::unique_ptr<web::FakeWebFrame> CreateFrame(bool is_main_frame) {
    std::unique_ptr<web::FakeWebFrame> frame = web::FakeWebFrame::Create(
        test::MakeLocalFrameToken().ToString(), is_main_frame, GURL());
    frame->set_browser_state(GetBrowserState());
    return frame;
  }

  std::unique_ptr<web::FakeWebFrame> CreateMainFrame() {
    return CreateFrame(/*is_main_frame=*/true);
  }

  std::unique_ptr<web::FakeWebFrame> CreateChildFrame() {
    return CreateFrame(/*is_main_frame=*/false);
  }

  web::WebFrame* AddFrame(std::unique_ptr<web::WebFrame> frame) {
    web::WebFrame* raw = frame.get();
    web_frames_manager().AddWebFrame(std::move(frame));
    return raw;
  }

  void RemoveFrame(const std::string& frame_id) {
    web_frames_manager().RemoveWebFrame(frame_id);
  }

  AutofillDriverIOSFactory& factory() {
    return *AutofillDriverIOSFactory::FromWebState(&web_state_);
  }

  MockWebFramesManagerObserver& pre_factory() { return pre_factory_; }

  MockWebFramesManagerObserver& post_factory() { return post_factory_; }

  MockAutofillDriverIOSFactoryObserver& factory_observer() {
    return factory_observer_;
  }

 private:
  web::ContentWorld content_world() {
    return AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
  }

  web::FakeWebFramesManager& web_frames_manager() {
    return static_cast<web::FakeWebFramesManager&>(
        *web_state_.GetWebFramesManager(content_world()));
  }

  test::AutofillUnitTestEnvironment autofill_environment_;
  MockWebFramesManagerObserver pre_factory_;
  MockWebFramesManagerObserver post_factory_;
  MockAutofillDriverIOSFactoryObserver factory_observer_;
  TestAutofillClient client_;
  web::FakeWebState web_state_;
};

enum class SourceOfRecursion {
  kOnAutofillDriverCreated,
  kOnAutofillDriverStateChanged
};

// The parameter specifies where the recursive events come from.
class AutofillDriverIOSFactoryTest_Recursion
    : public AutofillDriverIOSFactoryTest,
      public ::testing::WithParamInterface<SourceOfRecursion> {
 public:
  SourceOfRecursion source_of_recursion() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillDriverIOSFactoryTest,
    AutofillDriverIOSFactoryTest_Recursion,
    testing::Values(SourceOfRecursion::kOnAutofillDriverCreated,
                    SourceOfRecursion::kOnAutofillDriverStateChanged));

// Tests that DriverForFrame() tolerates recursive calls. These recursive calls
// come from AutofillDriverIOSFactory::Observer events.
TEST_P(AutofillDriverIOSFactoryTest_Recursion, RecursiveDriverForFrame) {
  // Creates 5 frames without associated drivers.
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  std::set<std::unique_ptr<web::WebFrame>> frames;
  std::set<AutofillDriverIOS*> drivers;
  while (frames.size() < 100) {
    frames.insert(CreateChildFrame());
  }

  // Creates all the drivers in recursive DriverForFrame() calls.
  auto create_drivers = [&](AutofillDriverIOSFactory& factory,
                            AutofillDriverIOS& driver, auto...) {
    for (auto& frame : frames) {
      drivers.insert(test_api(factory).DriverForFrame(frame.get()));
    }
  };
  MockAutofillDriverIOSFactoryObserver observer;
  factory().AddObserver(&observer);
  EXPECT_CALL(observer, OnAutofillDriverIOSCreated)
      .WillRepeatedly(create_drivers);
  EXPECT_CALL(observer, OnAutofillDriverIOSStateChanged)
      .WillRepeatedly(create_drivers);
  switch (source_of_recursion()) {
    case SourceOfRecursion::kOnAutofillDriverCreated:
      EXPECT_CALL(observer, OnAutofillDriverIOSCreated)
          .WillRepeatedly(create_drivers);
      break;
    case SourceOfRecursion::kOnAutofillDriverStateChanged:
      EXPECT_CALL(observer, OnAutofillDriverIOSStateChanged)
          .WillRepeatedly(create_drivers);
      break;
  }

  // Triggers the recursion.
  EXPECT_EQ(test_api(factory()).num_drivers(), 0u);
  EXPECT_EQ(frames.size(), 100u);
  EXPECT_EQ(drivers.size(), 0u);
  test_api(factory()).DriverForFrame(frames.begin()->get());
  EXPECT_EQ(frames.size(), 100u);
  EXPECT_EQ(drivers.size(), 100u);
  EXPECT_EQ(test_api(factory()).num_drivers(), 100u);
  factory().RemoveObserver(&observer);

  // Validate that the drivers are still registered.
  EXPECT_EQ(test_api(factory()).num_drivers(), 100u);
  EXPECT_THAT(frames,
              Each(Truly([&](const std::unique_ptr<web::WebFrame>& frame) {
                return drivers.contains(
                    test_api(factory()).DriverForFrame(frame.get()));
              })));
}

// Two helper macros to make tests below more readable.
#define EXPECT_DRIVER_CREATED(driver_ptr_ptr)                                  \
  EXPECT_CALL(factory_observer(),                                              \
              OnAutofillDriverIOSCreated(Ref(factory()), HasState(kInactive))) \
      .WillOnce(DoAll(SaveArgPtr<1>((driver_ptr_ptr))))
#define EXPECT_LIFECYCLE_CHANGE(driver_matcher, from, to)                  \
  EXPECT_CALL(factory_observer(),                                          \
              OnAutofillDriverIOSStateChanged(                             \
                  Ref(factory()), AllOf((driver_matcher), HasState((to))), \
                  (from), (to)))

// Tests that the basic lifecycle state changes of a driver.
TEST_F(AutofillDriverIOSFactoryTest, DriverLifecycle) {
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  web::WebFrame* frame = owned_frame.get();
  AutofillDriverIOS* driver = nullptr;

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(checkpoint, Call("DriverForFrame"));
    EXPECT_DRIVER_CREATED(&driver);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kInactive, kActive);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kActive, kPendingDeletion);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame));
  checkpoint.Call("DriverForFrame");
  factory().DriverForFrame(frame);
  checkpoint.Call("Unavailable");
  RemoveFrame(frame->GetFrameId());
  checkpoint.Call("Finish");
}

// Tests that only the first call of DriverForFrame() triggers events.
TEST_F(AutofillDriverIOSFactoryTest, DriverForFrameCalledMultipleTimes) {
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  web::WebFrame* frame = owned_frame.get();
  AutofillDriverIOS* driver = nullptr;

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(checkpoint, Call("DriverForFrame"));
    EXPECT_DRIVER_CREATED(&driver);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kInactive, kActive);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kActive, kPendingDeletion);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame));
  checkpoint.Call("DriverForFrame");
  factory().DriverForFrame(frame);
  factory().DriverForFrame(frame);
  factory().DriverForFrame(frame);
  checkpoint.Call("Unavailable");
  RemoveFrame(frame->GetFrameId());
  checkpoint.Call("Finish");
}

// Tests that the factory can manage multiple drivers.
TEST_F(AutofillDriverIOSFactoryTest, MultipleDrivers) {
  std::unique_ptr<web::WebFrame> owned_frame1 = CreateMainFrame();
  std::unique_ptr<web::WebFrame> owned_frame2 = CreateChildFrame();
  web::WebFrame* frame1 = owned_frame1.get();
  web::WebFrame* frame2 = owned_frame2.get();
  AutofillDriverIOS* driver1 = nullptr;
  AutofillDriverIOS* driver2 = nullptr;

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(checkpoint, Call("DriverForFrame"));
    EXPECT_DRIVER_CREATED(&driver1);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver1), kInactive, kActive);
    EXPECT_DRIVER_CREATED(&driver2);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kInactive, kActive);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kActive, kPendingDeletion);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver1), kActive, kPendingDeletion);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame1));
  AddFrame(std::move(owned_frame2));
  checkpoint.Call("DriverForFrame");
  factory().DriverForFrame(frame1);
  factory().DriverForFrame(frame2);
  checkpoint.Call("Unavailable");
  RemoveFrame(frame2->GetFrameId());
  RemoveFrame(frame1->GetFrameId());
  checkpoint.Call("Finish");
}

// Tests that the factory creates drivers lazily.
TEST_F(AutofillDriverIOSFactoryTest,
       DriverForFrameBetweenAvailableAndUnavailable) {
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  web::WebFrame* frame = owned_frame.get();
  AutofillDriverIOS* driver = nullptr;

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(pre_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(post_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(checkpoint, Call("DriverForFrame"));
    EXPECT_DRIVER_CREATED(&driver);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kInactive, kActive);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_CALL(pre_factory(), WebFrameBecameUnavailable);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kActive, kPendingDeletion);
    EXPECT_CALL(post_factory(), WebFrameBecameUnavailable);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame));
  checkpoint.Call("DriverForFrame");
  factory().DriverForFrame(frame);
  checkpoint.Call("Unavailable");
  RemoveFrame(frame->GetFrameId());
  checkpoint.Call("Finish");
}

// Tests that DriverForFrame() creates a driver when called *before* the factory
// has been notified about WebFrameBecameAvailable().
TEST_F(AutofillDriverIOSFactoryTest, DriverForFrameBeforeAvailable) {
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  web::WebFrame* frame = owned_frame.get();
  AutofillDriverIOS* driver = nullptr;

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(pre_factory(), WebFrameBecameAvailable).WillOnce([&] {
      factory().DriverForFrame(frame);
    });
    EXPECT_DRIVER_CREATED(&driver);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kInactive, kActive);
    EXPECT_CALL(post_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_CALL(pre_factory(), WebFrameBecameUnavailable);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kActive, kPendingDeletion);
    EXPECT_CALL(post_factory(), WebFrameBecameUnavailable);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame));
  checkpoint.Call("Unavailable");
  RemoveFrame(frame->GetFrameId());
  checkpoint.Call("Finish");
}

// Tests that DriverForFrame() becomes nullptr *after* the factory has been
// notified about WebFrameBecameUnavailable().
TEST_F(AutofillDriverIOSFactoryTest, DriverForFrameAfterUnavailable) {
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  web::WebFrame* frame = owned_frame.get();
  AutofillDriverIOS* driver = nullptr;

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(pre_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(post_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(checkpoint, Call("DriverForFrame"));
    EXPECT_DRIVER_CREATED(&driver);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kInactive, kActive);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_CALL(pre_factory(), WebFrameBecameUnavailable).WillOnce([&] {
      EXPECT_EQ(factory().DriverForFrame(frame), driver);
    });
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver), kActive, kPendingDeletion);
    EXPECT_CALL(post_factory(), WebFrameBecameUnavailable).WillOnce([&] {
      EXPECT_EQ(factory().DriverForFrame(frame), nullptr);
    });
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame));
  checkpoint.Call("DriverForFrame");
  factory().DriverForFrame(frame);
  checkpoint.Call("Unavailable");
  RemoveFrame(frame->GetFrameId());
  checkpoint.Call("Finish");
}

// Tests that DriverForFrame() returns nullptr when called *after* the factory
// has been notified about WebFrameBecameUnavailable() (i.e., when this is the
// only call).
TEST_F(AutofillDriverIOSFactoryTest, DriverForFrameAfterUnavailable2) {
  std::unique_ptr<web::WebFrame> owned_frame = CreateMainFrame();
  web::WebFrame* frame = owned_frame.get();

  MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(checkpoint, Call("Available"));
    EXPECT_CALL(pre_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(post_factory(), WebFrameBecameAvailable);
    EXPECT_CALL(checkpoint, Call("Unavailable"));
    EXPECT_CALL(pre_factory(), WebFrameBecameUnavailable);
    EXPECT_CALL(post_factory(), WebFrameBecameUnavailable).WillOnce([&] {
      EXPECT_EQ(factory().DriverForFrame(frame), nullptr);
    });
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Available");
  AddFrame(std::move(owned_frame));
  checkpoint.Call("Unavailable");
  RemoveFrame(frame->GetFrameId());
  checkpoint.Call("Finish");
}

}  // namespace
}  // namespace autofill
