// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_AURA_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_AURA_H_

#include "base/gtest_prod_util.h"
#include "components/ui_devtools/views/overlay_agent_views.h"

namespace ui_devtools {

class DOMAgent;

class OverlayAgentAura : public OverlayAgentViews {
 public:
  OverlayAgentAura(DOMAgent* dom_agent);

  OverlayAgentAura(const OverlayAgentAura&) = delete;
  OverlayAgentAura& operator=(const OverlayAgentAura&) = delete;

  ~OverlayAgentAura() override;

  int FindElementIdTargetedByPoint(ui::LocatedEvent* event) const override;
  static OverlayAgentAura* GetInstance() { return overlay_agent_aura_; }

 private:
  void InstallPreTargetHandler() override;
  void RemovePreTargetHandler() override;

  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest, HighlightWindow);
  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest, HighlightEmptyOrInvisibleWindow);

  static OverlayAgentAura* overlay_agent_aura_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_AURA_H_
