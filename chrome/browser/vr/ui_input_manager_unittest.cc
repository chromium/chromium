// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_content_input_delegate.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrictMock;

namespace vr {

namespace {

constexpr ControllerModel::ButtonState kUp = ControllerModel::ButtonState::kUp;
constexpr ControllerModel::ButtonState kDown =
    ControllerModel::ButtonState::kDown;

constexpr gfx::Size kWindowSize = {1280, 720};

class MockRect : public Rect {
 public:
  MockRect() = default;
  ~MockRect() override = default;

  MOCK_METHOD2(OnHoverEnter,
               void(const gfx::PointF& position, base::TimeTicks timestamp));
  MOCK_METHOD1(OnHoverLeave, void(base::TimeTicks timestamp));
  MOCK_METHOD2(OnHoverMove,
               void(const gfx::PointF& position, base::TimeTicks timestamp));
  MOCK_METHOD2(OnButtonDown,
               void(const gfx::PointF& position, base::TimeTicks timestamp));
  MOCK_METHOD2(OnButtonUp,
               void(const gfx::PointF& position, base::TimeTicks timestamp));
  MOCK_METHOD2(OnTouchMove,
               void(const gfx::PointF& position, base::TimeTicks timestamp));
  MOCK_METHOD2(OnScrollBegin,
               void(std::unique_ptr<InputEvent>, const gfx::PointF&));
  MOCK_METHOD2(OnScrollUpdate,
               void(std::unique_ptr<InputEvent>, const gfx::PointF&));
  MOCK_METHOD2(OnScrollEnd,
               void(std::unique_ptr<InputEvent>, const gfx::PointF&));
  MOCK_METHOD0(MockedOnScrollBegin, void());
  MOCK_METHOD1(OnFocusChanged, void(bool));
  MOCK_METHOD1(OnInputEdited, void(const EditedText&));
  MOCK_METHOD1(OnInputCommitted, void(const EditedText&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRect);
};

class MockTextInput : public TextInput {
 public:
  MockTextInput()
      : TextInput(1, base::RepeatingCallback<void(const EditedText&)>()) {}
  ~MockTextInput() override = default;

  MOCK_METHOD1(OnFocusChanged, void(bool));
  MOCK_METHOD1(OnInputEdited, void(const EditedText&));
  MOCK_METHOD1(OnInputCommitted, void(const EditedText&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTextInput);
};

class UiInputManagerTest : public testing::Test {
 public:
  void SetUp() override {
    scene_ = std::make_unique<UiScene>();
    input_manager_ = std::make_unique<UiInputManager>(scene_.get());
  }

  void TearDown() override { gesture_list_.clear(); }

  StrictMock<MockRect>* CreateAndAddMockElement(float z_position) {
    auto element = std::make_unique<StrictMock<MockRect>>();
    StrictMock<MockRect>* p_element = element.get();
    element->SetTranslate(0, 0, z_position);
    element->set_hit_testable(true);
    scene_->AddUiElement(kRoot, std::move(element));
    scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);
    return p_element;
  }

  StrictMock<MockTextInput>* CreateAndAddMockInputElement(float z_position) {
    auto element = std::make_unique<StrictMock<MockTextInput>>();
    StrictMock<MockTextInput>* p_element = element.get();
    element->SetTranslate(0, 0, z_position);
    element->SetSize(1, 0.1);
    element->set_hit_testable(true);
    scene_->AddUiElement(kRoot, std::move(element));
    scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);
    return p_element;
  }

  void HandleInput(const gfx::Vector3dF& laser_direction,
                   ControllerModel::ButtonState button_state) {
    HandleInput({0, 0, 0}, laser_direction, button_state);
  }

  void HandleInput(const gfx::Point3F& laser_origin,
                   const gfx::Vector3dF& laser_direction,
                   ControllerModel::ButtonState button_state) {
    RenderInfo render_info;
    controller_model_.laser_direction = laser_direction;
    controller_model_.laser_origin = laser_origin;
    controller_model_.touchpad_button_state = button_state;
    input_manager_->HandleInput(MsToTicks(1), render_info, controller_model_,
                                &reticle_model_, &gesture_list_);
  }

  void AddGesture(InputEvent::Type type) {
    auto gesture = std::make_unique<InputEvent>(type);
    gesture_list_.push_back(std::move(gesture));
  }

 protected:
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<UiInputManager> input_manager_;
  ReticleModel reticle_model_;
  ControllerModel controller_model_;
  InputEventList gesture_list_;
  InSequence inSequence;
};

class UiInputManagerContentTest : public UiTest {
 public:
  void SetUp() override {
    UiTest::SetUp();
    CreateScene(kNotInWebVr);
    input_manager_ = ui_instance_->input_manager();
  }

 protected:
  RenderInfo CreateRenderInfo() {
    RenderInfo render_info;
    gfx::Transform projection_matrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0,
                                     -1, 0.5);
    projection_matrix.Scale(
        1.0f, static_cast<float>(kWindowSize.width()) / kWindowSize.height());

    render_info.head_pose = head_pose_;
    render_info.left_eye_model.viewport = gfx::Rect(kWindowSize);
    render_info.left_eye_model.view_matrix = head_pose_;
    render_info.left_eye_model.proj_matrix = projection_matrix;
    render_info.left_eye_model.view_proj_matrix =
        projection_matrix * head_pose_;
    render_info.right_eye_model = render_info.left_eye_model;

    return render_info;
  }

  gfx::Transform head_pose_;
  UiInputManager* input_manager_;
};

TEST_F(UiInputManagerTest, FocusedElement) {
  StrictMock<MockTextInput>* p_element1 = CreateAndAddMockInputElement(-5.f);
  StrictMock<MockTextInput>* p_element2 = CreateAndAddMockInputElement(-5.f);
  EditedText edit(base::ASCIIToUTF16("asdfg"));

  // Focus request triggers OnFocusChanged.
  testing::Sequence s;
  EXPECT_CALL(*p_element1, OnFocusChanged(true)).InSequence(s);
  input_manager_->RequestFocus(p_element1->id());

  // Edit goes to focused element.
  EXPECT_CALL(*p_element1, OnInputEdited(edit)).InSequence(s);
  input_manager_->OnInputEdited(edit);

  // Commit goes to focused element.
  EXPECT_CALL(*p_element1, OnInputCommitted(edit)).InSequence(s);
  input_manager_->OnInputCommitted(edit);

  // Focus on a different element triggers OnFocusChanged.
  EXPECT_CALL(*p_element1, OnFocusChanged(false)).InSequence(s);
  EXPECT_CALL(*p_element2, OnFocusChanged(true)).InSequence(s);
  input_manager_->RequestFocus(p_element2->id());
}

// Verify that a focusable child clears focus off its parent. Note that the
// child isn't any different from other elements in that it should also steal
// focus from its parent.
TEST_F(UiInputManagerTest, FocusableChildStealsFocus) {
  StrictMock<MockRect>* p_element = CreateAndAddMockElement(-5.f);

  auto child = std::make_unique<StrictMock<MockRect>>();
  auto* p_child = child.get();
  child->set_hit_testable(true);
  child->set_focusable(true);
  p_element->AddChild(std::move(child));
  scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);

  // Focus element.
  testing::Sequence s;
  EXPECT_CALL(*p_element, OnFocusChanged(true)).InSequence(s);
  input_manager_->RequestFocus(p_element->id());

  // Focus child.
  EXPECT_CALL(*p_child, OnHoverEnter(_, _)).InSequence(s);
  EXPECT_CALL(*p_child, OnButtonDown(_, _)).InSequence(s);
  HandleInput(kForwardVector, kDown);
  EXPECT_CALL(*p_child, OnButtonUp(_, _)).InSequence(s);
  EXPECT_CALL(*p_element, OnFocusChanged(false)).InSequence(s);
  EXPECT_CALL(*p_child, OnHoverMove(_, _)).InSequence(s);
  HandleInput(kForwardVector, kUp);
}

// Verify that a non-focusable child does not clear focus off its parent.
TEST_F(UiInputManagerTest, NonFocusableChildDoestNotStealFocus) {
  StrictMock<MockRect>* p_element = CreateAndAddMockElement(-5.f);

  auto child = std::make_unique<StrictMock<MockRect>>();
  auto* p_child = child.get();
  child->set_hit_testable(true);
  child->set_focusable(false);
  p_element->AddChild(std::move(child));
  scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);

  // Focus element.
  testing::Sequence s;
  EXPECT_CALL(*p_element, OnFocusChanged(true)).InSequence(s);
  input_manager_->RequestFocus(p_element->id());

  // Focus child.
  EXPECT_CALL(*p_child, OnHoverEnter(_, _)).InSequence(s);
  EXPECT_CALL(*p_child, OnButtonDown(_, _)).InSequence(s);
  EXPECT_CALL(*p_element, OnFocusChanged(false)).Times(0).InSequence(s);
  HandleInput(kForwardVector, kDown);
}

TEST_F(UiInputManagerTest, ReticleRenderTarget) {
  auto element = std::make_unique<Rect>();
  UiElement* p_element = element.get();
  element->SetTranslate(0, 0, -1.f);
  element->set_hit_testable(true);
  scene_->AddUiElement(kRoot, std::move(element));
  scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);

  ControllerModel controller_model;
  controller_model.laser_direction = kBackwardVector;
  controller_model.laser_origin = {0, 0, 0};
  controller_model.touchpad_button_state = kUp;
  ReticleModel reticle_model;
  InputEventList input_event_list;

  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);
  EXPECT_EQ(0, reticle_model.target_element_id);

  controller_model.laser_direction = kForwardVector;
  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);
  EXPECT_EQ(p_element->id(), reticle_model.target_element_id);
  EXPECT_NEAR(-1.0, reticle_model.target_point.z(), kEpsilon);
}

// Test hover and click by toggling button state, and directing the controller
// either directly at (forward) or directly away from (backward) a test element.
// Verify mock expectations along the way to make failures easier to track.
TEST_F(UiInputManagerTest, HoverClick) {
  StrictMock<MockRect>* p_element = CreateAndAddMockElement(-5.f);

  // Move over the test element.
  EXPECT_CALL(*p_element, OnHoverEnter(_, _));
  HandleInput(kForwardVector, kUp);
  EXPECT_CALL(*p_element, OnHoverMove(_, _));
  HandleInput(kForwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Press the button while on the element.
  EXPECT_CALL(*p_element, OnHoverMove(_, _));
  EXPECT_CALL(*p_element, OnButtonDown(_, _));
  HandleInput(kForwardVector, kDown);
  Mock::VerifyAndClearExpectations(p_element);

  EXPECT_CALL(*p_element, OnTouchMove(_, _));
  HandleInput(kForwardVector, kDown);
  Mock::VerifyAndClearExpectations(p_element);

  // Release the button while on the element.
  EXPECT_CALL(*p_element, OnButtonUp(_, _));
  EXPECT_CALL(*p_element, OnHoverMove(_, _));
  HandleInput(kForwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Move off of the element.
  EXPECT_CALL(*p_element, OnHoverLeave(_));
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Press while not on the element, move over the element, move away, then
  // release. The element should not receive hover nor touch events.
  HandleInput(kBackwardVector, kDown);
  HandleInput(kForwardVector, kDown);
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Press on an element, move away, then release. The element should receive
  // hover leave, but keep receiving touch move events until release.
  EXPECT_CALL(*p_element, OnHoverEnter(_, _));
  EXPECT_CALL(*p_element, OnButtonDown(_, _));
  HandleInput(kForwardVector, kDown);
  EXPECT_CALL(*p_element, OnHoverLeave(_));
  EXPECT_CALL(*p_element, OnTouchMove(_, _));
  HandleInput(kBackwardVector, kDown);
  Mock::VerifyAndClearExpectations(p_element);
  EXPECT_CALL(*p_element, OnButtonUp(_, _));
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);
}

// Test pressing the button while on an element, moving to another element, and
// releasing the button. Before release, the first element should still receive
// touch move events. Upon release, the previous element should see its click
// and hover states cleared, and the new element should see a hover.
TEST_F(UiInputManagerTest, ReleaseButtonOnAnotherElement) {
  StrictMock<MockRect>* p_front_element = CreateAndAddMockElement(-5.f);
  StrictMock<MockRect>* p_back_element = CreateAndAddMockElement(5.f);

  // Press on an element.
  EXPECT_CALL(*p_front_element, OnHoverEnter(_, _));
  EXPECT_CALL(*p_front_element, OnButtonDown(_, _));
  HandleInput(kForwardVector, kDown);
  // Point to another element while pressing.
  EXPECT_CALL(*p_front_element, OnHoverLeave(_));
  EXPECT_CALL(*p_front_element, OnTouchMove(_, _));
  HandleInput(kBackwardVector, kDown);
  // Point again to the previous element while pressing.
  EXPECT_CALL(*p_front_element, OnTouchMove(_, _));
  EXPECT_CALL(*p_front_element, OnHoverEnter(_, _));
  HandleInput(kForwardVector, kDown);
  // Point to second element and release.
  EXPECT_CALL(*p_front_element, OnButtonUp(_, _));
  EXPECT_CALL(*p_front_element, OnHoverLeave(_));
  EXPECT_CALL(*p_back_element, OnHoverEnter(_, _));
  HandleInput(kBackwardVector, kUp);
  // Point again to first element.
  EXPECT_CALL(*p_back_element, OnHoverLeave(_));
  EXPECT_CALL(*p_front_element, OnHoverEnter(_, _));
  HandleInput(kForwardVector, kUp);
}

// Test scrolling while on an element, moving to another element, and stopping
// scrolling. Upon stop, the former element should have received the scrolling
// events, and the new element should receive the new scrolling events.
TEST_F(UiInputManagerTest, ScrollEndOnAnotherElement) {
  StrictMock<MockRect>* p_front_element = CreateAndAddMockElement(-5.0f);
  p_front_element->set_scrollable(true);
  StrictMock<MockRect>* p_back_element = CreateAndAddMockElement(5.0f);
  p_back_element->set_scrollable(true);

  // Scroll on an element.
  AddGesture(InputEvent::kScrollBegin);
  EXPECT_CALL(*p_front_element, OnHoverEnter(_, _));
  EXPECT_CALL(*p_front_element, OnScrollBegin(_, _));
  HandleInput(kForwardVector, kUp);
  EXPECT_TRUE(gesture_list_.empty());
  AddGesture(InputEvent::kScrollUpdate);
  EXPECT_CALL(*p_front_element, OnScrollUpdate(_, _));
  HandleInput(kForwardVector, kUp);
  EXPECT_TRUE(gesture_list_.empty());

  // Move away.
  AddGesture(InputEvent::kScrollUpdate);
  EXPECT_CALL(*p_front_element, OnHoverLeave(_));
  EXPECT_CALL(*p_front_element, OnScrollUpdate(_, _));
  HandleInput(kBackwardVector, kUp);
  EXPECT_TRUE(gesture_list_.empty());

  // Release scroll.
  AddGesture(InputEvent::kScrollEnd);
  EXPECT_CALL(*p_front_element, OnScrollEnd(_, _));
  EXPECT_CALL(*p_back_element, OnHoverEnter(_, _));
  HandleInput(kBackwardVector, kUp);
  EXPECT_TRUE(gesture_list_.empty());

  // Start scrolling on a new element.
  EXPECT_CALL(*p_back_element, OnHoverMove(_, _));
  AddGesture(InputEvent::kScrollBegin);
  EXPECT_CALL(*p_back_element, OnScrollBegin(_, _));
  HandleInput(kBackwardVector, kUp);
  EXPECT_TRUE(gesture_list_.empty());
}

// Test that scrolling events are transfered to the parent if the child is not
// scrollable.
TEST_F(UiInputManagerTest, ScrollBeginOnChild) {
  StrictMock<MockRect>* p_element = CreateAndAddMockElement(-5.0f);
  p_element->set_scrollable(true);
  auto child = std::make_unique<StrictMock<MockRect>>();
  auto* p_child = child.get();
  child->SetTranslate(0.0f, 0.0f, -5.0f);
  child->set_hit_testable(true);
  child->set_focusable(true);
  p_element->AddChild(std::move(child));

  AddGesture(InputEvent::kScrollBegin);
  EXPECT_CALL(*p_element, OnHoverEnter(_, _));
  EXPECT_CALL(*p_element, OnScrollBegin(_, _));
  EXPECT_CALL(*p_child, OnScrollBegin(_, _)).Times(0);
  HandleInput(kForwardVector, kUp);
  ASSERT_EQ(p_child->id(), reticle_model_.target_element_id);
  EXPECT_TRUE(gesture_list_.empty());
}

// Test that input is tolerant of disappearing elements.
TEST_F(UiInputManagerTest, ElementDeletion) {
  StrictMock<MockRect>* p_element = CreateAndAddMockElement(-5.f);

  // Hover on an element.
  EXPECT_CALL(*p_element, OnHoverEnter(_, _));
  HandleInput(kForwardVector, kUp);

  // Remove and retain the element from the scene, and ensure that it receives
  // no extraneous input.
  auto deleted_element = scene_->RemoveUiElement(p_element->id());
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Re-add the element to the scene, and press on it to lock it for input.
  scene_->AddUiElement(kRoot, std::move(deleted_element));
  scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);
  EXPECT_CALL(*p_element, OnHoverEnter(_, _));
  EXPECT_CALL(*p_element, OnButtonDown(_, _));
  HandleInput(kForwardVector, kDown);

  // Remove the element again, move off the element, and release to ensure that
  // input isn't delivered to an input-locked element that's been deleted from
  // the scene.
  scene_->RemoveUiElement(p_element->id());
  HandleInput(kBackwardVector, kDown);
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);
}

// This ensures that the input manager can support both the strategy of hit
// testing along the laser's ray as well as the ray from the world origin to a
// point far along the laser.
TEST_F(UiInputManagerTest, HitTestStrategy) {
  auto element = std::make_unique<Rect>();
  auto* p_element = element.get();
  element->SetTranslate(0, 0, -2.5);
  element->SetSize(1000.0f, 1000.0f);
  element->set_hit_testable(true);
  scene_->AddUiElement(kRoot, std::move(element));
  scene_->OnBeginFrame(base::TimeTicks(), kStartHeadPose);

  gfx::Point3F center = p_element->GetCenter();
  gfx::Point3F laser_origin(0.5, -0.5, 0.0);

  HandleInput(laser_origin, center - laser_origin, kDown);

  ASSERT_NE(0, reticle_model_.target_element_id);
  EXPECT_EQ(p_element->id(), reticle_model_.target_element_id);
  EXPECT_POINT3F_EQ(gfx::Point3F(-0.45f, 0.45f, -2.5f),
                    reticle_model_.target_point);

  input_manager_->set_hit_test_strategy(
      UiInputManager::PROJECT_TO_LASER_ORIGIN_FOR_TEST);

  HandleInput(laser_origin, center - laser_origin, kDown);

  ASSERT_NE(0, reticle_model_.target_element_id);
  EXPECT_EQ(p_element->id(), reticle_model_.target_element_id);
  EXPECT_POINT3F_EQ(gfx::Point3F(0.0f, 0.0, -2.5f),
                    reticle_model_.target_point);
}

TEST_F(UiInputManagerContentTest, NoMouseMovesDuringClick) {
  EXPECT_FALSE(RunForMs(500));
  // It would be nice if the controller weren't platform specific and we could
  // mock out the underlying sensor data. For now, we will hallucinate
  // parameters to HandleInput.
  UiElement* content_quad =
      scene_->GetUiElementByName(UiElementName::kContentQuad);
  gfx::Point3F content_quad_center = content_quad->GetCenter();
  gfx::Point3F origin;

  ControllerModel controller_model;
  controller_model.laser_direction = content_quad_center - origin;
  controller_model.laser_origin = origin;
  controller_model.touchpad_button_state = ControllerModel::ButtonState::kDown;
  ReticleModel reticle_model;
  InputEventList input_event_list;
  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);

  // We should have hit the content quad if our math was correct.
  ASSERT_NE(0, reticle_model.target_element_id);
  EXPECT_EQ(content_quad->id(), reticle_model.target_element_id);

  // Unless we suppress content move events during clicks, this will cause us to
  // call OnContentMove on the delegate. We should do this suppression, so we
  // set the expected number of calls to zero.
  EXPECT_CALL(*content_input_delegate_, OnHoverMove(_, _)).Times(0);

  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);
}

TEST_F(UiInputManagerContentTest, AudioPermissionPromptHitTesting) {
  model_->active_modal_prompt_type =
      kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission;
  EXPECT_TRUE(RunForMs(500));

  UiElement* url_bar = scene_->GetUiElementByName(UiElementName::kUrlBar);
  gfx::Point3F url_bar_center = url_bar->GetCenter();
  gfx::Point3F origin;

  ControllerModel controller_model;
  controller_model.laser_direction = url_bar_center - origin;
  controller_model.laser_origin = origin;
  controller_model.touchpad_button_state = ControllerModel::ButtonState::kDown;
  ReticleModel reticle_model;
  InputEventList input_event_list;
  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);

  // Even if the reticle is over the URL bar, the backplane should be in front
  // and should be hit.
  ASSERT_NE(0, reticle_model.target_element_id);
  auto* prompt = scene_->GetUiElementByName(kExitPrompt);
  auto* backplane = prompt->GetDescendantByType(kTypePromptBackplane);
  EXPECT_EQ(backplane->id(), reticle_model.target_element_id);
}

TEST_F(UiInputManagerContentTest, TreeVsZOrder) {
  // It would be nice if the controller weren't platform specific and we could
  // mock out the underlying sensor data. For now, we will hallucinate
  // parameters to HandleInput.
  UiElement* content_quad =
      scene_->GetUiElementByName(UiElementName::kContentQuad);
  gfx::Point3F content_quad_center = content_quad->GetCenter();
  gfx::Point3F origin;

  ControllerModel controller_model;
  controller_model.laser_direction = content_quad_center - origin;
  controller_model.laser_origin = origin;
  controller_model.touchpad_button_state = ControllerModel::ButtonState::kDown;
  ReticleModel reticle_model;
  InputEventList input_event_list;
  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);

  // We should have hit the content quad if our math was correct.
  ASSERT_NE(0, reticle_model.target_element_id);
  EXPECT_EQ(content_quad->id(), reticle_model.target_element_id);

  // We will now move the content quad behind the backplane.
  content_quad->SetTranslate(0, 0, -1.0);
  AdvanceFrame();

  input_manager_->HandleInput(MsToTicks(1), RenderInfo(), controller_model,
                              &reticle_model, &input_event_list);

  // We should have hit the content quad even though, geometrically, it stacks
  // behind the backplane.
  ASSERT_NE(0, reticle_model.target_element_id);
  EXPECT_EQ(content_quad->id(), reticle_model.target_element_id);
}

TEST_F(UiInputManagerContentTest, ControllerRestingInViewport) {
  gfx::Point3F controller_center(0.5f, 0.5f, 0.f);

  head_pose_ = gfx::Transform(
      gfx::Quaternion(kForwardVector, controller_center - kOrigin));

  ControllerModel controller_model;
  controller_model.laser_direction = kForwardVector;
  controller_model.transform.Translate3d(
      controller_center.x(), controller_center.y(), controller_center.z());
  controller_model.laser_origin = controller_center;
  ReticleModel reticle_model;
  InputEventList input_event_list;
  RenderInfo render_info = CreateRenderInfo();

  // The controller is initially not in the viewport.
  EXPECT_FALSE(input_manager_->ControllerRestingInViewport());

  std::vector<ControllerModel> controllers;

  input_manager_->HandleInput(MsToTicks(1), render_info, controller_model,
                              &reticle_model, &input_event_list);
  controllers.push_back(controller_model);
  ui_->OnControllersUpdated(controllers, reticle_model);
  scene_->OnBeginFrame(base::TimeTicks(), head_pose_);

  // Although we are currently looking at the controller, it is not focused yet.
  // It must remain in the viewport for the requisite amount of time.
  EXPECT_FALSE(input_manager_->ControllerRestingInViewport());

  input_manager_->HandleInput(MsToTicks(50000), render_info, controller_model,
                              &reticle_model, &input_event_list);
  controllers[0] = controller_model;
  ui_->OnControllersUpdated(controllers, reticle_model);
  scene_->OnBeginFrame(base::TimeTicks(), head_pose_);

  // Since the controller has been in the viewport for a long time (50s), it
  // must report that it is focused.
  EXPECT_TRUE(input_manager_->ControllerRestingInViewport());

  controllers[0] = controller_model;
  ui_->OnControllersUpdated(controllers, reticle_model);
  scene_->OnBeginFrame(base::TimeTicks(), head_pose_);

  EXPECT_TRUE(model_->controllers[0].resting_in_viewport);

  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
}

}  // namespace

}  // namespace vr
