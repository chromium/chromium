// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIZ_FRAME_SINK_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIZ_FRAME_SINK_ELEMENT_H_

#include "base/macros.h"
#include "components/ui_devtools/viz/viz_element.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace viz {
class FrameSinkManagerImpl;
}

namespace ui_devtools {

class FrameSinkElement : public VizElement {
 public:
  FrameSinkElement(const viz::FrameSinkId& frame_sink_id,
                   viz::FrameSinkManagerImpl* frame_sink_manager,
                   UIElementDelegate* ui_element_delegate,
                   UIElement* parent,
                   bool is_root,
                   bool has_created_frame_sink);
  ~FrameSinkElement() override;

  // Used by DOMAgentViz on updates when element is already present
  // in a tree but its properties need to be changed.
  void SetHasCreatedFrameSink(bool has_created_frame_sink) {
    has_created_frame_sink_ = has_created_frame_sink;
  }
  void SetRoot(bool is_root) { is_root_ = is_root; }

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
  bool has_created_frame_sink() const { return has_created_frame_sink_; }

  // UIElement:
  std::vector<UIElement::ClassProperties> GetCustomPropertiesForMatchedStyle()
      const override;
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  std::vector<std::string> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndScreenBounds()
      const override;

  static const viz::FrameSinkId& From(const UIElement* element);

 private:
  const viz::FrameSinkId frame_sink_id_;
  viz::FrameSinkManagerImpl* frame_sink_manager_;

  // Properties of the FrameSink. If element is a RootFrameSink then it has
  // |is_root_| = true. If element is not a root than it has |is_root_| = false.
  // If an element is a sibling of a RootFrameSink but has property |is_root_| =
  // false then it is considered detached.
  bool is_root_;
  bool has_created_frame_sink_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIZ_FRAME_SINK_ELEMENT_H_
