// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/plist_writer.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "third_party/libxml/chromium/xml_writer.h"

namespace policy {

namespace {

// Called recursively to build the Plist xml. When completed,
// |plist_writer| will contain the Plist. Return true on success and false on
// failure.
bool BuildPlistString(const base::Value& node, XmlWriter& plist_writer) {
  switch (node.type()) {
    case base::Value::Type::BOOLEAN: {
      bool value = node.GetBool();
      plist_writer.StartElement(value ? "true" : "false");
      plist_writer.EndElement();
      return true;
    }

    case base::Value::Type::INTEGER: {
      int value = node.GetInt();
      plist_writer.WriteElement("integer", base::NumberToString(value));
      return true;
    }

    case base::Value::Type::STRING: {
      std::string value = node.GetString();
      plist_writer.WriteElement("string", value);
      return true;
    }

    case base::Value::Type::LIST: {
      plist_writer.StartElement("array");

      for (const auto& value : node.GetList()) {
        if (!BuildPlistString(value, plist_writer))
          return false;
      }

      plist_writer.EndElement();
      return true;
    }

    case base::Value::Type::DICTIONARY: {
      plist_writer.StartElement("dict");

      const base::DictionaryValue* dict = nullptr;
      bool result = node.GetAsDictionary(&dict);
      DCHECK(result);
      for (base::DictionaryValue::Iterator itr(*dict); !itr.IsAtEnd();
           itr.Advance()) {
        plist_writer.WriteElement("key", itr.key());

        if (!BuildPlistString(itr.value(), plist_writer))
          result = false;
      }

      plist_writer.EndElement();
      return result;
    }

    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

bool PlistWrite(const base::Value& node, std::string* plist) {
  // Where we write Plist data as we generate it.
  XmlWriter plist_writer;
  plist_writer.StartWriting();
  plist_writer.StartIndenting();
  plist_writer.StartElement("plist");
  bool result = BuildPlistString(node, plist_writer);
  plist_writer.EndElement();
  plist_writer.StopWriting();

  *plist = plist_writer.GetWrittenString();
  return result;
}

}  // namespace policy
