// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_ROOT_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_ROOT_ELEMENT_H_

#include "components/ui_devtools/ui_element.h"

namespace ui_devtools {

class UI_DEVTOOLS_EXPORT RootElement : public UIElement {
 public:
  explicit RootElement(UIElementDelegate* ui_element_delegate);

  RootElement(const RootElement&) = delete;
  RootElement& operator=(const RootElement&) = delete;

  ~RootElement() override;

  // UIElement:
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;
};
}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_ROOT_ELEMENT_H_
