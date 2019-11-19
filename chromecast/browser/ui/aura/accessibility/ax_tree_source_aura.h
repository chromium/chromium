// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_
#define CHROMECAST_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/accessibility/ax_tree_source_views.h"

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes.
class AXTreeSourceAura : public views::AXTreeSourceViews {
 public:
  AXTreeSourceAura(views::AXAuraObjWrapper* root,
                   const ui::AXTreeID& tree_id,
                   views::AXAuraObjCache* cache);
  ~AXTreeSourceAura() override;

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  void SerializeNode(views::AXAuraObjWrapper* node,
                     ui::AXNodeData* out_data) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceAura);
};

#endif  // CHROMECAST_BROWSER_UI_AURA_ACCESSIBILITY_AX_TREE_SOURCE_AURA_H_
