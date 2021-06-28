// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_TEST_H_
#define CHROME_BROWSER_VR_TEST_UI_TEST_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/mock_content_input_delegate.h"
#include "chrome/browser/vr/test/mock_ui_browser_interface.h"
#include "chrome/browser/vr/ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class UiElement;
class UiScene;
struct Model;

class UiTest : public testing::Test {
 public:
  UiTest();
  ~UiTest() override;

  void SetUp() override;

 protected:
  enum InWebVr : bool {
    kNotInWebVr = false,
    kInWebVr = true,
  };

  enum WebVrAutopresented : bool {
    kNotAutopresented = false,
    kAutopresented = true,
  };

  void CreateScene(const UiInitialState& state);
  void CreateScene(InWebVr in_web_vr);

 protected:
  void CreateSceneInternal(
      const UiInitialState& state,
      std::unique_ptr<MockContentInputDelegate> content_input_delegate);

  void SetIncognito(bool incognito);

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

  // Count the number of elements in the named element's subtree.
  int NumVisibleInTree(UiElementName name) const;

  // Return false if not all elements in the set match the specified |animating|
  // state for the specified |properties|. Other elements are ignored.
  bool VerifyIsAnimating(const std::set<UiElementName>& names,
                         const std::vector<TargetProperty>& properties,
                         bool animating) const;

  // Return false if not all elements in the set match the specified requires
  // layout state. Other elements are ignored.
  bool VerifyRequiresLayout(const std::set<UiElementName>& names,
                            bool requires_layout) const;

  // Check if element is using correct opacity in Render recursively.
  void CheckRendererOpacityRecursive(UiElement* element);

  // Advances current_time_ by delta. This is done by running the next frame,
  // then jumping time ahead to the final time. Generally, the UI should not
  // require all intermediate frames to be called. Tests that require this
  // should simulate the required intermediate frames.
  bool RunForMs(float milliseconds);
  bool RunForSeconds(float seconds);

  // Advances time by one frame (16 ms) and calls 'OnBeginFrame()'
  bool AdvanceFrame();

  void GetBackgroundColor(SkColor* background_color) const;

  // Synthesize a controller orientation that intersects the element, and cycle
  // the controller button.  This offers a reasonably correct means of testing
  // clicks on elements, that's true to hit testability, visbility, etc.
  void ClickElement(UiElement* element);

  std::unique_ptr<Ui> ui_instance_;
  UiInterface* ui_ = nullptr;
  std::unique_ptr<MockUiBrowserInterface> browser_;
  MockContentInputDelegate* content_input_delegate_ = nullptr;
  Model* model_ = nullptr;
  UiScene* scene_ = nullptr;

 private:
  bool RunFor(base::TimeDelta delta);
  bool OnBeginFrame() const;

  base::TimeTicks current_time_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_TEST_H_
