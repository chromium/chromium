// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIZ_VIZ_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIZ_VIZ_ELEMENT_H_

#include "base/macros.h"
#include "components/ui_devtools/ui_element.h"

namespace ui_devtools {

class VizElement : public UIElement {
 public:
  ~VizElement() override;

  // Insert this into the list of |parent|'s children, comparing to siblings to
  // find an appropriate insert point. |parent| isn't necessarily a VizElement,
  // but each of its children should be so that they can be compared.
  void AddToParentSorted(UIElement* parent, bool notify_delegate = true);

  // Move to become a child of |new_parent|. The DOM node for this element is
  // destroyed and recreated in the new location.
  void Reparent(UIElement* new_parent);

  static VizElement* AsVizElement(UIElement* element);

 protected:
  VizElement(const UIElementType type,
             UIElementDelegate* delegate,
             UIElement* parent);

 private:
  DISALLOW_COPY_AND_ASSIGN(VizElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIZ_VIZ_ELEMENT_H_
