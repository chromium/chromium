// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/ax_tree_source_views.h"

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes.
class AXTreeSourceAura : public views::AXTreeSourceViews {
 public:
  AXTreeSourceAura();
  ~AXTreeSourceAura() override;

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  views::AXAuraObjWrapper* GetRoot() const override;
  void SerializeNode(views::AXAuraObjWrapper* node,
                     ui::AXNodeData* out_data) const override;

 private:
  // A root object representing the entire desktop.
  std::unique_ptr<AXRootObjWrapper> desktop_root_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceAura);
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_
