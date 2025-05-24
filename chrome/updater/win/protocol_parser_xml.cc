// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/protocol_parser_xml.h"

// clang-format off
#include <objbase.h>
#include <msxml2.h>
// clang-format on
#include <wrl/client.h>

#include <cstdint>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "url/gurl.h"

namespace updater {
namespace {

bool ReadAttribute(IXMLDOMNode* node,
                   const std::wstring& attribute_name,
                   BSTR* value) {
  Microsoft::WRL::ComPtr<IXMLDOMNamedNodeMap> attributes;
  HRESULT hr = node->get_attributes(&attributes);
  if (FAILED(hr) || !attributes) {
    return false;
  }

  Microsoft::WRL::ComPtr<IXMLDOMNode> attribute_node;
  base::win::ScopedVariant node_value;
  base::win::ScopedBstr attr_name(attribute_name);

  hr = attributes->getNamedItem(attr_name.Get(), &attribute_node);
  if (FAILED(hr) || !attribute_node) {
    return false;
  }

  hr = attribute_node->get_nodeValue(node_value.Receive());
  if (FAILED(hr) || node_value.type() == VT_EMPTY) {
    return false;
  }

  CHECK_EQ(node_value.type(), VT_BSTR);
  VARIANT released_variant = node_value.Release();
  *value = V_BSTR(&released_variant);
  return true;
}

bool ReadStringAttribute(IXMLDOMNode* node,
                         const std::wstring& attribute_name,
                         std::string* value) {
  base::win::ScopedBstr node_value;
  if (!ReadAttribute(node, attribute_name, node_value.Receive())) {
    return false;
  }

  *value = base::SysWideToUTF8(node_value.Get());
  return true;
}

}  // namespace

bool ProtocolParserXML::ParseAction(IXMLDOMNode* node, Results* results) {
  std::string event;
  if (!ReadStringAttribute(node, L"event", &event)) {
    VLOG(1) << "Missing `event` attribute in <action>";
    return false;
  }

  if (event == "install") {
    if (!results->apps.back().manifest.run.empty()) {
      // Only parse the first install action and ignore the rest.
      return true;
    }

    if (!ReadStringAttribute(node, L"run",
                             &results->apps.back().manifest.run) ||
        results->apps.back().manifest.run.empty()) {
      VLOG(1) << "Missing `run` attribute in <action>";
      return false;
    }

    ReadStringAttribute(node, L"arguments",
                        &results->apps.back().manifest.arguments);
    return true;
  } else if (event == "postinstall") {
    // The "postinstall" event is handled by `AppInstall`. The common
    // `postinstall` scenario where the installer provides a launch command is
    // handled by `AppInstall` by running the launch command and exiting in the
    // interactive install case.
    // Other `postinstall` actions such as "reboot", "restart browser", and
    // "post install url" are obsolete, since no one currently uses these
    // actions.
    return true;
  } else {
    VLOG(1) << "Unsupported `event` type in <action>: %s", event.c_str();
    return false;
  }
}

bool ProtocolParserXML::ParseActions(IXMLDOMNode* node, Results* results) {
  return ParseChildren(
      {
          {L"action", &ProtocolParserXML::ParseAction},
      },
      node, results);
}

bool ProtocolParserXML::ParseApp(IXMLDOMNode* node, Results* results) {
  App result;
  if (!ReadStringAttribute(node, L"appid", &result.app_id)) {
    VLOG(1) << "Missing `appid` attribute in <app>";
    return false;
  }

  results->apps.push_back(result);

  return ParseChildren(
      {
          {L"data", &ProtocolParserXML::ParseData},
          {L"updatecheck", &ProtocolParserXML::ParseUpdateCheck},
      },
      node, results);
}

bool ProtocolParserXML::ParseData(IXMLDOMNode* node, Results* results) {
  Data data;
  if (!ReadStringAttribute(node, L"index", &data.install_data_index)) {
    VLOG(1) << "Missing `index` attribute in <data>";
    return false;
  }

  base::win::ScopedBstr text;
  HRESULT hr = node->get_text(text.Receive());
  if (FAILED(hr)) {
    VLOG(1) << "Failed to get text from <data>: " << hr;
    return false;
  }
  data.text = base::SysWideToUTF8(text.Get());

  results->apps.back().data.push_back(data);
  return true;
}

bool ProtocolParserXML::ParseManifest(IXMLDOMNode* node, Results* results) {
  std::string version;
  if (ReadStringAttribute(node, L"version", &version) &&
      base::Version(version).IsValid()) {
    results->apps.back().manifest.version = version;
  }

  return ParseChildren(
      {
          {L"actions", &ProtocolParserXML::ParseActions},
      },
      node, results);
}

bool ProtocolParserXML::ParseResponse(IXMLDOMNode* node, Results* results) {
  std::string protocol_version;
  if (!ReadStringAttribute(node, L"protocol", &protocol_version) ||
      protocol_version != "3.0") {
    VLOG(1) << "Unsupported protocol version: %s", protocol_version.c_str();
    return false;
  }

  return ParseChildren(
      {
          {L"app", &ProtocolParserXML::ParseApp},
          {L"systemrequirements", &ProtocolParserXML::ParseSystemRequirements},
      },
      node, results);
}

bool ProtocolParserXML::ParseSystemRequirements(IXMLDOMNode* node,
                                                Results* results) {
  std::string platform;
  if (ReadStringAttribute(node, L"platform", &platform)) {
    results->system_requirements.platform = platform;
  }

  std::string arch;
  if (ReadStringAttribute(node, L"arch", &arch)) {
    results->system_requirements.arch = arch;
  }

  std::string min_os_version;
  if (ReadStringAttribute(node, L"min_os_version", &min_os_version)) {
    results->system_requirements.min_os_version = min_os_version;
  }

  return true;
}

bool ProtocolParserXML::ParseUpdateCheck(IXMLDOMNode* node, Results* results) {
  const ElementHandlerMap child_handlers = {
      {L"manifest", &ProtocolParserXML::ParseManifest},
  };
  return ParseChildren(child_handlers, node, results);
}

bool ProtocolParserXML::ParseElement(const ElementHandlerMap& handler_map,
                                     IXMLDOMNode* node,
                                     Results* results) {
  base::win::ScopedBstr basename;

  if (FAILED(node->get_baseName(basename.Receive()))) {
    VLOG(1) << "Failed to get name for a DOM element.";
    return false;
  }

  for (const auto& [name, handler] : handler_map) {
    if (name == basename.Get()) {
      return (this->*handler)(node, results);
    }
  }

  return true;  // Ignore unknown elements.
}

bool ProtocolParserXML::ParseChildren(const ElementHandlerMap& handler_map,
                                      IXMLDOMNode* node,
                                      Results* results) {
  Microsoft::WRL::ComPtr<IXMLDOMNodeList> children_list;

  HRESULT hr = node->get_childNodes(&children_list);
  if (FAILED(hr)) {
    VLOG(1) << "Failed to get child elements: " << hr;
    return false;
  }

  long num_children = 0;
  hr = children_list->get_length(&num_children);
  if (FAILED(hr)) {
    VLOG(1) << "Failed to get the number of child elements: " << hr;
    return false;
  }

  for (long i = 0; i < num_children; ++i) {
    Microsoft::WRL::ComPtr<IXMLDOMNode> child_node;
    hr = children_list->get_item(i, &child_node);
    if (FAILED(hr)) {
      VLOG(1) << "Failed to get " << i << "-th child element: " << hr;
      return false;
    }

    DOMNodeType type = NODE_INVALID;
    hr = child_node->get_nodeType(&type);
    if (FAILED(hr)) {
      VLOG(1) << "Failed to get " << i << "-th child element type: " << hr;
      return false;
    }

    if (type == NODE_ELEMENT &&
        !ParseElement(handler_map, child_node.Get(), results)) {
      return false;
    }
  }

  return true;
}

std::optional<ProtocolParserXML::Results> ProtocolParserXML::DoParse(
    const std::string& response_xml) {
  Microsoft::WRL::ComPtr<IXMLDOMDocument> xmldoc;
  HRESULT hr = ::CoCreateInstance(CLSID_DOMDocument30, nullptr, CLSCTX_ALL,
                                  IID_IXMLDOMDocument, &xmldoc);
  if (FAILED(hr)) {
    VLOG(1) << "IXMLDOMDocument.CoCreateInstance failed: " << hr;
    return std::nullopt;
  }
  hr = xmldoc->put_resolveExternals(VARIANT_FALSE);
  if (FAILED(hr)) {
    VLOG(1) << "IXMLDOMDocument.put_resolveExternals failed: " << hr;
    return std::nullopt;
  }

  VARIANT_BOOL is_successful(VARIANT_FALSE);
  auto xml_bstr =
      base::win::ScopedBstr(base::SysUTF8ToWide(response_xml).c_str());
  hr = xmldoc->loadXML(xml_bstr.Get(), &is_successful);
  if (FAILED(hr)) {
    VLOG(1) << "Load manifest failed: " << hr;
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IXMLDOMElement> root_node;
  hr = xmldoc->get_documentElement(&root_node);
  if (FAILED(hr) || !root_node) {
    VLOG(1) << "Load manifest failed: " << hr;
    return std::nullopt;
  }

  ProtocolParserXML::Results results;
  if (!ParseElement({{L"response", &ProtocolParserXML::ParseResponse}},
                    root_node.Get(), &results)) {
    return std::nullopt;
  }
  return results;
}

ProtocolParserXML::App::App() = default;
ProtocolParserXML::App::App(const App&) = default;
ProtocolParserXML::App::~App() = default;
ProtocolParserXML::Results::Results() = default;
ProtocolParserXML::Results::Results(const Results&) = default;
ProtocolParserXML::Results::~Results() = default;

std::optional<ProtocolParserXML::Results> ProtocolParserXML::Parse(
    const std::string& xml) {
  return ProtocolParserXML().DoParse(xml);
}

}  // namespace updater
