// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_TEXT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_TEXT_INPUT_DELEGATE_H_

#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockTextInputDelegate : public TextInputDelegate {
 public:
  MockTextInputDelegate();

  MockTextInputDelegate(const MockTextInputDelegate&) = delete;
  MockTextInputDelegate& operator=(const MockTextInputDelegate&) = delete;

  ~MockTextInputDelegate() override;

  MOCK_METHOD1(UpdateInput, void(const TextInputInfo& info));
  MOCK_METHOD1(RequestFocus, void(int));
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_TEXT_INPUT_DELEGATE_H_
