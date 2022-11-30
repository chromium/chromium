// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_UI_ELEMENT_WITH_METADATA_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_UI_ELEMENT_WITH_METADATA_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/ui_devtools/ui_element.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"

namespace ui_devtools {

class UIElementWithMetaData : public UIElement {
 public:
  UIElementWithMetaData(const UIElementWithMetaData&) = delete;
  UIElementWithMetaData& operator=(const UIElementWithMetaData&) = delete;
  ~UIElementWithMetaData() override;

  // UIElement:
  std::vector<UIElement::ClassProperties> GetCustomPropertiesForMatchedStyle()
      const override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  bool SetPropertiesFromString(const std::string& text) override;
  void InitSources() override;

 protected:
  UIElementWithMetaData(const UIElementType type,
                        UIElementDelegate* delegate,
                        UIElement* parent);

  // Returns the metadata for the class instance type for this specific element.
  virtual ui::metadata::ClassMetaData* GetClassMetaData() const = 0;
  // Returns an opaque pointer for the actual instance which this element
  // represents.
  virtual void* GetClassInstance() const = 0;
  // Returns the layer for the given element if one exists. Returns null if no
  // layer is currently available.
  virtual ui::Layer* GetLayer() const;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_UI_ELEMENT_WITH_METADATA_H_
