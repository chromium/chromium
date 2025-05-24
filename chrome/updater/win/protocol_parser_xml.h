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
#include <optional>
#include <string>

#include "base/containers/flat_map.h"

namespace updater {

struct OfflineManifestSystemRequirements {
  std::string platform;  // For example, "win".

  // Expected host processor architecture that the app is compatible with.
  // `arch` can be a single entry, or multiple entries separated with `,`.
  // Entries prefixed with a `-` (negative entries) indicate non-compatible
  // hosts.
  //
  // Examples:
  // * `arch` == "x86".
  // * `arch` == "x64".
  // * `arch` == "x86,x64,-arm64": the app will fail installation if the
  // underlying host is arm64.
  std::string arch;

  std::string min_os_version;  // major.minor.
};

// Class that parses the XML manifest for offline installers.
//
// Only features that are used by Windows legacy offline installers are parsed.
class ProtocolParserXML final {
 public:
  struct Manifest {
    std::string version;
    std::string run;
    std::string arguments;
  };

  struct Data {
    std::string install_data_index;
    std::string text;
  };

  struct App {
    App();
    App(const App& app);
    ~App();

    std::string app_id;
    Manifest manifest;
    std::vector<Data> data;
  };

  struct Results {
    Results();
    Results(const Results& results);
    ~Results();

    OfflineManifestSystemRequirements system_requirements;
    std::vector<App> apps;
  };

  static std::optional<Results> Parse(const std::string& xml);

  ProtocolParserXML(const ProtocolParserXML&) = delete;
  ProtocolParserXML& operator=(const ProtocolParserXML&) = delete;

 private:
  ProtocolParserXML() = default;

  using ElementHandler = bool (ProtocolParserXML::*)(IXMLDOMNode* node,
                                                     Results* results);
  using ElementHandlerMap = base::flat_map<std::wstring, ElementHandler>;

  std::optional<Results> DoParse(const std::string& response_xml);

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
  bool ParseResponse(IXMLDOMNode* node, Results* results);
  bool ParseSystemRequirements(IXMLDOMNode* node, Results* results);
  bool ParseUpdateCheck(IXMLDOMNode* node, Results* results);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_PROTOCOL_PARSER_XML_H_
