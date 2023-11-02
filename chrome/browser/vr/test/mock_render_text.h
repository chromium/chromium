// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_RENDER_TEXT_H_
#define CHROME_BROWSER_VR_TEST_MOCK_RENDER_TEXT_H_

#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockRenderText : public RenderTextWrapper {
 public:
  MockRenderText();

  MockRenderText(const MockRenderText&) = delete;
  MockRenderText& operator=(const MockRenderText&) = delete;

  ~MockRenderText() override;

  MOCK_METHOD1(SetColor, void(SkColor value));
  MOCK_METHOD2(ApplyColor, void(SkColor value, const gfx::Range& range));
  MOCK_METHOD2(SetStyle, void(gfx::TextStyle style, bool value));
  MOCK_METHOD3(ApplyStyle,
               void(gfx::TextStyle style, bool value, const gfx::Range& range));
  MOCK_METHOD1(SetWeight, void(gfx::Font::Weight weight));
  MOCK_METHOD2(ApplyWeight,
               void(gfx::Font::Weight weight, const gfx::Range& range));
  MOCK_METHOD1(SetDirectionalityMode, void(gfx::DirectionalityMode mode));
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_RENDER_TEXT_H_
