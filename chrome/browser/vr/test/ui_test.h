// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_TEST_H_
#define CHROME_BROWSER_VR_TEST_UI_TEST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class UiScene;
struct Model;

class UiTest : public testing::Test {
 public:
  UiTest();
  ~UiTest() override;

  void SetUp() override;

 protected:
  // Check whether a named element is visible. In this test, visibilility is the
  // target visibily, not the current (possibly animating) visibility. This
  // makes it easier to test the visibility of elements in response to state
  // changes.
  bool IsVisible(UiElementName name) const;

  // Return false if not all elements in the set match the specified visibility
  // state. Other elements are ignored.
  bool VerifyVisibility(const std::set<UiElementName>& names,
                        bool visible) const;

  // Check that only a specific set of elements is visible, and others are not.
  void VerifyOnlyElementsVisible(const std::string& trace_context,
                                 const std::set<UiElementName>& names) const;

  // Advances current_time_ by delta. This is done by running the next frame,
  // then jumping time ahead to the final time. Generally, the UI should not
  // require all intermediate frames to be called. Tests that require this
  // should simulate the required intermediate frames.
  bool RunForMs(float milliseconds);
  bool RunForSeconds(float seconds);

  // Advances time by one frame (16 ms) and calls 'OnBeginFrame()'
  bool AdvanceFrame();

  std::unique_ptr<Ui> ui_instance_;
  raw_ptr<UiInterface, DanglingUntriaged> ui_ = nullptr;
  raw_ptr<Model, DanglingUntriaged> model_ = nullptr;
  raw_ptr<UiScene, DanglingUntriaged> scene_ = nullptr;

 private:
  bool RunFor(base::TimeDelta delta);
  bool OnBeginFrame() const;

  base::TimeTicks current_time_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_TEST_H_
