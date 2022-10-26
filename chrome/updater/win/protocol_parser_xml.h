// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_PROTOCOL_PARSER_XML_H_
#define CHROME_UPDATER_WIN_PROTOCOL_PARSER_XML_H_

// clang-format off
#include <objbase.h>
#include <msxml2.h>
// clang-format on

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "components/update_client/protocol_parser.h"

namespace updater {

// Class that parses the XML manifest for offline installers.
//
// Only features that are used by Windows legacy offline installers AND those
// supported by `ProtocolParser` (components/update_client/protocol_parser.h)
// are implemented. Element types that are not (fully) handled:
//   * All elements related with diff update are ignored.
//   * Parse fails if <package> only has SHA1 hash.
//   * Only the first manifest action is honored and the rest are ignored.
//   * All elements related with cohort are ignored.
//
class ProtocolParserXML final : public update_client::ProtocolParser {
 public:
  ProtocolParserXML() = default;

  ProtocolParserXML(const ProtocolParserXML&) = delete;
  ProtocolParserXML& operator=(const ProtocolParserXML&) = delete;

 private:
  using ElementHandler =
      bool (ProtocolParserXML::*)(IXMLDOMNode* node,
                                  ProtocolParserXML::Results* results);
  using ElementHandlerMap = base::flat_map<std::wstring, ElementHandler>;

  // Overrides for ProtocolParser.
  bool DoParse(const std::string& response_xml, Results* results) override;

  // Helper functions to traverse the XML document.
  bool ParseElement(const ElementHandlerMap& handler_map,
                    IXMLDOMNode* node,
                    Results* results);
  bool ParseChildren(const ElementHandlerMap& handler_map,
                     IXMLDOMNode* node,
                     Results* results);

  // DOM element handlers.
  bool ParseAction(IXMLDOMNode* node, Results* results);
  bool ParseActions(IXMLDOMNode* node, Results* results);
  bool ParseApp(IXMLDOMNode* node, Results* results);
  bool ParseData(IXMLDOMNode* node, Results* results);
  bool ParseEvent(IXMLDOMNode* node, Results* results);
  bool ParseManifest(IXMLDOMNode* node, Results* results);
  bool ParsePackage(IXMLDOMNode* node, Results* results);
  bool ParsePackages(IXMLDOMNode* node, Results* results);
  bool ParsePing(IXMLDOMNode* node, Results* results);
  bool ParseResponse(IXMLDOMNode* node, Results* results);
  bool ParseSystemRequirements(IXMLDOMNode* node, Results* results);
  bool ParseUpdateCheck(IXMLDOMNode* node, Results* results);
  bool ParseUrl(IXMLDOMNode* node, Results* results);
  bool ParseUrls(IXMLDOMNode* node, Results* results);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_PROTOCOL_PARSER_XML_H_
