// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_CSS_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_CSS_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/css.h"
#include "components/ui_devtools/dom_agent.h"

namespace gfx {
class Rect;
}

namespace ui_devtools {

class UIElement;

class UI_DEVTOOLS_EXPORT CSSAgent
    : public UiDevToolsBaseAgent<protocol::CSS::Metainfo>,
      public DOMAgentObserver {
 public:
  explicit CSSAgent(DOMAgent* dom_agent);

  CSSAgent(const CSSAgent&) = delete;
  CSSAgent& operator=(const CSSAgent&) = delete;

  ~CSSAgent() override;

  // CSS::Backend:
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getMatchedStylesForNode(
      int node_id,
      protocol::Maybe<protocol::Array<protocol::CSS::RuleMatch>>*
          matched_css_rules) override;
  protocol::Response getStyleSheetText(const protocol::String& style_sheet_id,
                                       protocol::String* text) override;
  protocol::Response setStyleTexts(
      std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>
          edits,
      std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* result)
      override;

  // DOMAgentObserver:
  void OnElementBoundsChanged(UIElement* ui_element) override;

 private:
  std::unique_ptr<protocol::CSS::CSSStyle> GetStylesForUIElement(
      UIElement* ui_element);
  void InvalidateStyleSheet(UIElement* ui_element);
  bool GetPropertiesForUIElement(UIElement* ui_element,
                                 gfx::Rect* bounds,
                                 bool* visible);
  bool SetPropertiesForUIElement(UIElement* ui_element,
                                 const gfx::Rect& bounds,
                                 bool visible);
  std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>> BuildMatchedStyles(
      UIElement* ui_element);

  // Sends header to frontend for each section in properties panel.
  void InitStylesheetHeaders(UIElement* ui_element);

  const raw_ptr<DOMAgent> dom_agent_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_CSS_AGENT_H_
