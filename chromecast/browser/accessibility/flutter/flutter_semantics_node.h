// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_SEMANTICS_NODE_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_SEMANTICS_NODE_H_

#include <string>
#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace chromecast {
namespace accessibility {

// FlutterSemanticsNode represents a single flutter semantics object from
// flutter. This class is used by AXTreeSourceFlutter to encapsulate
// flutter information which maps to a single AXNodeData.
class FlutterSemanticsNode {
 public:
  virtual ~FlutterSemanticsNode() = default;

  virtual int32_t GetId() const = 0;
  virtual const gfx::Rect GetBounds() const = 0;
  virtual bool IsVisibleToUser() const = 0;
  virtual bool IsFocused() const = 0;
  virtual bool IsLiveRegion() const = 0;
  virtual bool HasScopesRoute() const = 0;
  virtual bool HasNamesRoute() const = 0;
  virtual bool IsRapidChangingSlider() const = 0;
  virtual bool CanBeAccessibilityFocused() const = 0;
  virtual void PopulateAXRole(ui::AXNodeData* out_data) const = 0;
  virtual void PopulateAXState(ui::AXNodeData* out_data) const = 0;
  virtual void Serialize(ui::AXNodeData* out_data) const = 0;
  virtual void GetChildren(
      std::vector<FlutterSemanticsNode*>* children) const = 0;
  virtual void ComputeNameFromContents(
      std::vector<std::string>* names) const = 0;
  virtual bool HasLabelHint() const = 0;
  virtual std::string GetLabelHint() const = 0;
  virtual bool HasValue() const = 0;
  virtual std::string GetValue() const = 0;
  virtual bool IsKeyboardNode() const = 0;
};

}  // namespace accessibility
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_SEMANTICS_NODE_H_
