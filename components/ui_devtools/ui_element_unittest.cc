// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/ui_element.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui_devtools {

class FakeUIElementDelegate : public UIElementDelegate {
 public:
  ~FakeUIElementDelegate() override = default;
  void OnUIElementAdded(UIElement* parent, UIElement* child) override {
    // This itself is hacky; `OnUIElementAdded` is currently called once when
    // we create the element, and again when it's parented.
    // `DOMAgent::OnUIElementAdded` treats the first add as a completely
    // different code path where it writes the node to the map, then bails.
    // NB: the *second* call, where parent is not null does not write to the
    // map. Since we're not doing a larger refactor right now, just replicate
    // this logic.
    if (!parent) {
      elements_.push_back(child);
    }
  }

  void OnUIElementReordered(UIElement* parent, UIElement* child) override {}

  void OnUIElementRemoved(UIElement* ui_element) override {
    std::erase(elements_, ui_element);
  }

  void OnUIElementBoundsChanged(UIElement* ui_element) override {}

  const std::vector<raw_ptr<UIElement, VectorExperimental>>& elements() {
    return elements_;
  }

 private:
  std::vector<raw_ptr<UIElement, VectorExperimental>> elements_;
};

using UIElementsUnittest = testing::Test;

TEST_F(UIElementsUnittest, ClearChildrenTellsDelegate) {
  FakeUIElementDelegate delegate;
  FakeUIElement root = FakeUIElement(&delegate);
  ASSERT_EQ(delegate.elements().size(), 1u);
  EXPECT_EQ(delegate.elements().back(), &root);

  FakeUIElement* a = new FakeUIElement(&delegate);
  FakeUIElement* b = new FakeUIElement(&delegate);
  root.AddChild(a);
  root.AddChild(b);
  EXPECT_EQ(delegate.elements().size(), 3u);

  root.ClearChildren();
  ASSERT_EQ(delegate.elements().size(), 1u);
  EXPECT_EQ(delegate.elements().back(), &root);
}

}  // namespace ui_devtools
