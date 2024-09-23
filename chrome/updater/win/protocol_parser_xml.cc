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

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
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

bool ReadInt64Attribute(IXMLDOMNode* node,
                        const std::wstring& attribute_name,
                        int64_t* value) {
  base::win::ScopedBstr node_value;
  return ReadAttribute(node, attribute_name, node_value.Receive()) &&
         base::StringToInt64(node_value.Get(), value);
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
    ParseError("Missing `event` attribute in <action>");
    return false;
  }

  if (event == "install") {
    if (!results->list.back().manifest.run.empty()) {
      // Only parse the first install action and ignore the rest.
      return true;
    }

    if (!ReadStringAttribute(node, L"run",
                             &results->list.back().manifest.run) ||
        results->list.back().manifest.run.empty()) {
      ParseError("Missing `run` attribute in <action>");
      return false;
    }

    ReadStringAttribute(node, L"arguments",
                        &results->list.back().manifest.arguments);
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
    ParseError("Unsupported `event` type in <action>: %s", event.c_str());
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
  Result result;
  if (!ReadStringAttribute(node, L"appid", &result.extension_id)) {
    ParseError("Missing `appid` attribute in <app>");
    return false;
  }

  if (!ReadStringAttribute(node, L"status", &result.status)) {
    ParseError("Missing `status` attribute in <app>");
    return false;
  }

  if (result.status != "ok") {
    ParseError("Unexpected app status: %s", result.status.c_str());
    return false;
  }
  results->list.push_back(result);

  return ParseChildren(
      {
          {L"data", &ProtocolParserXML::ParseData},
          {L"updatecheck", &ProtocolParserXML::ParseUpdateCheck},
      },
      node, results);
}

bool ProtocolParserXML::ParseData(IXMLDOMNode* node, Results* results) {
  ProtocolParser::Result::Data data;
  if (!ReadStringAttribute(node, L"status", &data.status)) {
    ParseError("Missing `status` attribute in <data>");
    return false;
  }
  if (!ReadStringAttribute(node, L"index", &data.install_data_index)) {
    ParseError("Missing `index` attribute in <data>");
    return false;
  }
  if (!ReadStringAttribute(node, L"name", &data.name)) {
    ParseError("Missing `name` attribute in <data>");
    return false;
  }

  base::win::ScopedBstr text;
  HRESULT hr = node->get_text(text.Receive());
  if (FAILED(hr)) {
    ParseError("Failed to get text from <data>: %#x", hr);
    return false;
  }
  data.text = base::SysWideToUTF8(text.Get());

  results->list.back().data.push_back(data);
  return true;
}

bool ProtocolParserXML::ParseManifest(IXMLDOMNode* node, Results* results) {
  std::string version;
  if (ReadStringAttribute(node, L"version", &version) &&
      base::Version(version).IsValid()) {
    results->list.back().manifest.version = version;
  }

  return ParseChildren(
      {
          {L"actions", &ProtocolParserXML::ParseActions},
          {L"packages", &ProtocolParserXML::ParsePackages},
      },
      node, results);
}

bool ProtocolParserXML::ParsePackage(IXMLDOMNode* node, Results* results) {
  ProtocolParser::Result::Manifest::Package p;

  if (!ReadStringAttribute(node, L"name", &p.name) || p.name.empty()) {
    ParseError("Missing `name` attribute in <package>");
    return false;
  }

  if (!ReadStringAttribute(node, L"hash_sha256", &p.hash_sha256)) {
    ParseError("Missing `hash_sha256` attribute in <package>");
    return false;
  }

  if (!ReadInt64Attribute(node, L"size", &p.size)) {
    ParseError("Missing `size` attribute in <package>");
    return false;
  }

  ReadStringAttribute(node, L"fp", &p.fingerprint);

  results->list.back().manifest.packages.push_back(p);
  return true;
}

bool ProtocolParserXML::ParsePackages(IXMLDOMNode* node, Results* results) {
  return ParseChildren(
      {
          {L"package", &ProtocolParserXML::ParsePackage},
      },
      node, results);
}

bool ProtocolParserXML::ParseResponse(IXMLDOMNode* node, Results* results) {
  std::string protocol_version;
  if (!ReadStringAttribute(node, L"protocol", &protocol_version) ||
      protocol_version != "3.0") {
    ParseError("Unsupported protocol version: %s", protocol_version.c_str());
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
  Result& result = results->list.back();

  if (!ReadStringAttribute(node, L"status", &result.status)) {
    ParseError("Missing `status` attribute in <updatecheck>");
    return false;
  }

  const ElementHandlerMap child_handlers = {
      {L"manifest", &ProtocolParserXML::ParseManifest},
      {L"urls", &ProtocolParserXML::ParseUrls},
  };
  return ParseChildren(child_handlers, node, results);
}

bool ProtocolParserXML::ParseUrl(IXMLDOMNode* node, Results* results) {
  std::string url_string;
  if (!ReadStringAttribute(node, L"codebase", &url_string)) {
    ParseError("Missing `codebase` attribute in <url>");
    return false;
  }

  GURL url(url_string);
  if (!url.is_valid()) {
    ParseError("Invalid URL codebase in <url>: %s", url_string.c_str());
    return false;
  }

  results->list.back().crx_urls.push_back(url);
  return true;
}

bool ProtocolParserXML::ParseUrls(IXMLDOMNode* node, Results* results) {
  return ParseChildren(
      {
          {L"url", &ProtocolParserXML::ParseUrl},
      },
      node, results);
}

bool ProtocolParserXML::ParseElement(const ElementHandlerMap& handler_map,
                                     IXMLDOMNode* node,
                                     Results* results) {
  base::win::ScopedBstr basename;

  if (FAILED(node->get_baseName(basename.Receive()))) {
    ParseError("Failed to get name for a DOM element.");
    return false;
  }

  for (const auto& [name, handler] : handler_map) {
    if (name == basename.Get()) {
      return (this->*handler)(node, results);
    }
  }

  ParseError("Unrecognized element: %s",
             base::SysWideToUTF8(basename.Get()).c_str());
  return false;
}

bool ProtocolParserXML::ParseChildren(const ElementHandlerMap& handler_map,
                                      IXMLDOMNode* node,
                                      Results* results) {
  Microsoft::WRL::ComPtr<IXMLDOMNodeList> children_list;

  HRESULT hr = node->get_childNodes(&children_list);
  if (FAILED(hr)) {
    ParseError("Failed to get child elements: %#x", hr);
    return false;
  }

  long num_children = 0;
  hr = children_list->get_length(&num_children);
  if (FAILED(hr)) {
    ParseError("Failed to get the number of child elements: %#x", hr);
    return false;
  }

  for (long i = 0; i < num_children; ++i) {
    Microsoft::WRL::ComPtr<IXMLDOMNode> child_node;
    hr = children_list->get_item(i, &child_node);
    if (FAILED(hr)) {
      ParseError("Failed to get %d-th child element: %#x", i, hr);
      return false;
    }

    DOMNodeType type = NODE_INVALID;
    hr = child_node->get_nodeType(&type);
    if (FAILED(hr)) {
      ParseError("Failed to get %d-th child element type: %#x", i, hr);
      return false;
    }

    if (type == NODE_ELEMENT &&
        !ParseElement(handler_map, child_node.Get(), results)) {
      return false;
    }
  }

  return true;
}

bool ProtocolParserXML::DoParse(const std::string& response_xml,
                                Results* results) {
  CHECK(results);

  Microsoft::WRL::ComPtr<IXMLDOMDocument> xmldoc;
  HRESULT hr = ::CoCreateInstance(CLSID_DOMDocument30, nullptr, CLSCTX_ALL,
                                  IID_IXMLDOMDocument, &xmldoc);
  if (FAILED(hr)) {
    ParseError("IXMLDOMDocument.CoCreateInstance failed: %#x", hr);
    return false;
  }
  hr = xmldoc->put_resolveExternals(VARIANT_FALSE);
  if (FAILED(hr)) {
    ParseError("IXMLDOMDocument.put_resolveExternals failed: %#x", hr);
    return false;
  }

  VARIANT_BOOL is_successful(VARIANT_FALSE);
  auto xml_bstr =
      base::win::ScopedBstr(base::SysUTF8ToWide(response_xml).c_str());
  hr = xmldoc->loadXML(xml_bstr.Get(), &is_successful);
  if (FAILED(hr)) {
    ParseError("Load manifest failed: %#x", hr);
    return false;
  }

  Microsoft::WRL::ComPtr<IXMLDOMElement> root_node;
  hr = xmldoc->get_documentElement(&root_node);
  if (FAILED(hr) || !root_node) {
    ParseError("Load manifest failed: %#x", hr);
    return false;
  }

  return ParseElement(
      {
          {L"response", &ProtocolParserXML::ParseResponse},
      },
      root_node.Get(), results);
}

}  // namespace updater
