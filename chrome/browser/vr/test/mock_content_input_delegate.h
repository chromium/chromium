// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_CONTENT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_CONTENT_INPUT_DELEGATE_H_

#include "chrome/browser/vr/content_input_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockContentInputDelegate : public ContentInputDelegate {
 public:
  MockContentInputDelegate();
  ~MockContentInputDelegate() override;

  MOCK_METHOD2(OnHoverEnter,
               void(const gfx::PointF& normalized_hit_point,
                    base::TimeTicks timestamp));
  MOCK_METHOD1(OnHoverLeave, void(base::TimeTicks timestamp));
  MOCK_METHOD2(OnHoverMove,
               void(const gfx::PointF& normalized_hit_point,
                    base::TimeTicks timestamp));
  MOCK_METHOD2(OnButtonDown,
               void(const gfx::PointF& normalized_hit_point,
                    base::TimeTicks timestamp));
  MOCK_METHOD2(OnButtonUp,
               void(const gfx::PointF& normalized_hit_point,
                    base::TimeTicks timestamp));

  // As move-only parameters aren't supported by mock methods, we will override
  // the functions explicitly and fwd the calls to the mocked functions.
  MOCK_METHOD2(FwdContentInputEvent,
               void(std::unique_ptr<InputEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));

  void OnInputEvent(std::unique_ptr<InputEvent> gesture,
                    const gfx::PointF& normalized_hit_point) override {
    FwdContentInputEvent(gesture, normalized_hit_point);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_CONTENT_INPUT_DELEGATE_H_
