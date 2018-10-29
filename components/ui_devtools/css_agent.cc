// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/css_agent.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/ui_devtools/ui_element.h"

namespace ui_devtools {
namespace {

using namespace ui_devtools::protocol;

const char kHeight[] = "height";
const char kWidth[] = "width";
const char kX[] = "x";
const char kY[] = "y";
const char kVisibility[] = "visibility";

std::unique_ptr<CSS::SourceRange> BuildDefaultSourceRange() {
  // These tell the frontend where in the stylesheet a certain style
  // is located. Since we don't have stylesheets, this is all 0.
  // We need this because CSS fields are not editable unless
  // the range is provided.
  return CSS::SourceRange::create()
      .setStartLine(0)
      .setEndLine(0)
      .setStartColumn(0)
      .setEndColumn(0)
      .build();
}

std::unique_ptr<CSS::CSSProperty> BuildCSSProperty(const std::string& name,
                                                   int value) {
  return CSS::CSSProperty::create()
      .setRange(BuildDefaultSourceRange())
      .setName(name)
      .setValue(base::IntToString(value))
      .build();
}

std::unique_ptr<CSS::CSSProperty> BuildCSSProperty(const std::string& name,
                                                   const std::string& value) {
  return CSS::CSSProperty::create()
      .setRange(BuildDefaultSourceRange())
      .setName(name)
      .setValue(value)
      .build();
}

std::unique_ptr<Array<CSS::CSSProperty>> BuildCSSPropertyArray(
    const gfx::Rect& bounds,
    const bool visible) {
  auto cssProperties = Array<CSS::CSSProperty>::create();
  cssProperties->addItem(BuildCSSProperty(kHeight, bounds.height()));
  cssProperties->addItem(BuildCSSProperty(kWidth, bounds.width()));
  cssProperties->addItem(BuildCSSProperty(kX, bounds.x()));
  cssProperties->addItem(BuildCSSProperty(kY, bounds.y()));
  cssProperties->addItem(BuildCSSProperty(kVisibility, visible));
  return cssProperties;
}

std::unique_ptr<CSS::CSSStyle> BuildCSSStyle(UIElement* ui_element) {
  gfx::Rect bounds;
  bool visible;
  ui_element->GetBounds(&bounds);
  ui_element->GetVisible(&visible);

  std::unique_ptr<Array<CSS::CSSProperty>> css_properties(
      BuildCSSPropertyArray(bounds, visible));
  const std::vector<std::pair<std::string, std::string>> properties(
      ui_element->GetCustomProperties());

  for (const auto& it : properties)
    css_properties->addItem(BuildCSSProperty(it.first, it.second));

  return CSS::CSSStyle::create()
      .setRange(BuildDefaultSourceRange())
      .setStyleSheetId(base::IntToString(ui_element->node_id()))
      .setCssProperties(std::move(css_properties))
      .setShorthandEntries(Array<protocol::CSS::ShorthandEntry>::create())
      .build();
}

Response NodeNotFoundError(int node_id) {
  return Response::Error("Node with id=" + std::to_string(node_id) +
                         " not found");
}

Response ParseProperties(const std::string& style_text,
                         gfx::Rect* bounds,
                         bool* visible) {
  std::vector<std::string> tokens = base::SplitString(
      style_text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < tokens.size() - 1; i += 2) {
    const std::string& property = tokens.at(i);
    int value;
    if (!base::StringToInt(tokens.at(i + 1), &value))
      return Response::Error("Unable to parse value for property=" + property);

    if (property == kHeight)
      bounds->set_height(std::max(0, value));
    else if (property == kWidth)
      bounds->set_width(std::max(0, value));
    else if (property == kX)
      bounds->set_x(value);
    else if (property == kY)
      bounds->set_y(value);
    else if (property == kVisibility)
      *visible = std::max(0, value) == 1;
    else
      return Response::Error("Unsupported property=" + property);
  }
  return Response::OK();
}

}  // namespace

CSSAgent::CSSAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

CSSAgent::~CSSAgent() {
  disable();
}

Response CSSAgent::enable() {
  dom_agent_->AddObserver(this);
  return Response::OK();
}

Response CSSAgent::disable() {
  dom_agent_->RemoveObserver(this);
  return Response::OK();
}

Response CSSAgent::getMatchedStylesForNode(int node_id,
                                           Maybe<CSS::CSSStyle>* inline_style) {
  UIElement* ui_element = dom_agent_->GetElementFromNodeId(node_id);
  *inline_style = GetStylesForUIElement(ui_element);
  if (!inline_style)
    return NodeNotFoundError(node_id);
  return Response::OK();
}

Response CSSAgent::setStyleTexts(
    std::unique_ptr<Array<CSS::StyleDeclarationEdit>> edits,
    std::unique_ptr<Array<CSS::CSSStyle>>* result) {
  std::unique_ptr<Array<CSS::CSSStyle>> updated_styles =
      Array<CSS::CSSStyle>::create();
  for (size_t i = 0; i < edits->length(); i++) {
    auto* edit = edits->get(i);
    int node_id;
    if (!base::StringToInt(edit->getStyleSheetId(), &node_id))
      return Response::Error("Invalid node id");

    UIElement* ui_element = dom_agent_->GetElementFromNodeId(node_id);
    gfx::Rect updated_bounds;
    bool visible = false;
    if (!GetPropertiesForUIElement(ui_element, &updated_bounds, &visible))
      return NodeNotFoundError(node_id);

    Response response(
        ParseProperties(edit->getText(), &updated_bounds, &visible));
    if (!response.isSuccess())
      return response;

    if (!SetPropertiesForUIElement(ui_element, updated_bounds, visible))
      return NodeNotFoundError(node_id);

    updated_styles->addItem(BuildCSSStyle(ui_element));
  }
  *result = std::move(updated_styles);
  return Response::OK();
}

void CSSAgent::OnElementBoundsChanged(UIElement* ui_element) {
  InvalidateStyleSheet(ui_element);
}

std::unique_ptr<CSS::CSSStyle> CSSAgent::GetStylesForUIElement(
    UIElement* ui_element) {
  gfx::Rect bounds;
  bool visible = false;
  return GetPropertiesForUIElement(ui_element, &bounds, &visible)
             ? BuildCSSStyle(ui_element)
             : nullptr;
}

void CSSAgent::InvalidateStyleSheet(UIElement* ui_element) {
  // The stylesheetId for each node is equivalent to its node_id (as a string).
  frontend()->styleSheetChanged(base::IntToString(ui_element->node_id()));
}

bool CSSAgent::GetPropertiesForUIElement(UIElement* ui_element,
                                         gfx::Rect* bounds,
                                         bool* visible) {
  if (ui_element) {
    ui_element->GetBounds(bounds);
    ui_element->GetVisible(visible);
    return true;
  }
  return false;
}

bool CSSAgent::SetPropertiesForUIElement(UIElement* ui_element,
                                         const gfx::Rect& bounds,
                                         bool visible) {
  if (ui_element) {
    ui_element->SetBounds(bounds);
    ui_element->SetVisible(visible);
    return true;
  }
  return false;
}

}  // namespace ui_devtools
