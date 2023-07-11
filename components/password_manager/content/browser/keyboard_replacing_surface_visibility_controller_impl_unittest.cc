// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class MockRenderWidgetHost : public content::RenderWidgetHost {
 public:
  MOCK_METHOD(void,
              AddSuppressShowingImeCallback,
              (const content::RenderWidgetHost::SuppressShowingImeCallback&),
              (override));
  MOCK_METHOD(void,
              RemoveSuppressShowingImeCallback,
              (const content::RenderWidgetHost::SuppressShowingImeCallback&),
              (override));
  MOCK_METHOD(const viz::FrameSinkId&, GetFrameSinkId, (), (override));
  MOCK_METHOD(void,
              UpdateTextDirection,
              (base::i18n::TextDirection),
              (override));
  MOCK_METHOD(void, NotifyTextDirection, (), (override));
  MOCK_METHOD(void, Focus, (), (override));
  MOCK_METHOD(void, Blur, (), (override));
  MOCK_METHOD(void, FlushForTesting, (), (override));
  MOCK_METHOD(void, SetActive, (bool), (override));
  MOCK_METHOD(void,
              ForwardMouseEvent,
              (const blink::WebMouseEvent&),
              (override));
  MOCK_METHOD(void,
              ForwardWheelEvent,
              (const blink::WebMouseWheelEvent&),
              (override));
  MOCK_METHOD(void,
              ForwardKeyboardEvent,
              (const content::NativeWebKeyboardEvent&),
              (override));
  MOCK_METHOD(void,
              ForwardKeyboardEventWithLatencyInfo,
              (const content::NativeWebKeyboardEvent&, const ui::LatencyInfo&),
              (override));
  MOCK_METHOD(void,
              ForwardGestureEvent,
              (const blink::WebGestureEvent&),
              (override));
  MOCK_METHOD(content::RenderProcessHost*, GetProcess, (), (override));
  MOCK_METHOD(int, GetRoutingID, (), (override));
  MOCK_METHOD(content::RenderWidgetHostView*, GetView, (), (override));
  MOCK_METHOD(bool, IsCurrentlyUnresponsive, (), (override));
  MOCK_METHOD(bool, SynchronizeVisualProperties, (), (override));
  MOCK_METHOD(void,
              AddKeyPressEventCallback,
              (const KeyPressEventCallback&),
              (override));
  MOCK_METHOD(void,
              RemoveKeyPressEventCallback,
              (const KeyPressEventCallback&),
              (override));
  MOCK_METHOD(void,
              AddMouseEventCallback,
              (const MouseEventCallback&),
              (override));
  MOCK_METHOD(void,
              RemoveMouseEventCallback,
              (const MouseEventCallback&),
              (override));
  MOCK_METHOD(void, AddInputEventObserver, (InputEventObserver*), (override));
  MOCK_METHOD(void,
              RemoveInputEventObserver,
              (InputEventObserver*),
              (override));
  MOCK_METHOD(void,
              AddImeInputEventObserver,
              (InputEventObserver*),
              (override));
  MOCK_METHOD(void,
              RemoveImeInputEventObserver,
              (InputEventObserver*),
              (override));
  MOCK_METHOD(void,
              AddObserver,
              (content::RenderWidgetHostObserver*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (content::RenderWidgetHostObserver*),
              (override));
  MOCK_METHOD(display::ScreenInfo, GetScreenInfo, (), (const override));
  MOCK_METHOD(display::ScreenInfos, GetScreenInfos, (), (const override));
  MOCK_METHOD(float, GetDeviceScaleFactor, (), (override));
  MOCK_METHOD(absl::optional<cc::TouchAction>,
              GetAllowedTouchAction,
              (),
              (override));
  MOCK_METHOD(void, WriteIntoTrace, (perfetto::TracedValue), (override));
};

TEST(KeyboardReplacingSurfaceVisibilityControllerImplTest, Visibility) {
  base::test::ScopedFeatureList enable_feature(
      features::kPasswordSuggestionBottomSheetV2);
  KeyboardReplacingSurfaceVisibilityControllerImpl controller;
  MockRenderWidgetHost widget_host;

  EXPECT_TRUE(controller.CanBeShown());

  EXPECT_CALL(widget_host, AddSuppressShowingImeCallback(testing::_)).Times(1);
  controller.SetVisible(&widget_host);

  EXPECT_TRUE(controller.IsVisible());

  // The state is already `kVisible`. Do not retry to show.
  EXPECT_CALL(widget_host, AddSuppressShowingImeCallback(testing::_)).Times(0);
  controller.SetVisible(&widget_host);
}

TEST(KeyboardReplacingSurfaceVisibilityControllerImplTest, Reset) {
  base::test::ScopedFeatureList enable_feature(
      features::kPasswordSuggestionBottomSheetV2);
  KeyboardReplacingSurfaceVisibilityControllerImpl controller;
  MockRenderWidgetHost widget_host;

  EXPECT_TRUE(controller.CanBeShown());

  controller.SetVisible(&widget_host);

  EXPECT_FALSE(controller.CanBeShown());

  EXPECT_CALL(widget_host, RemoveSuppressShowingImeCallback(testing::_))
      .Times(1);
  controller.Reset();

  EXPECT_TRUE(controller.CanBeShown());
}

}  // namespace password_manager
