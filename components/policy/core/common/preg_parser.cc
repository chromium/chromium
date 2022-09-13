// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/preg_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/registry_dict.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
// Registry data type constants.
#define REG_NONE 0
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD_LITTLE_ENDIAN 4
#define REG_DWORD_BIG_ENDIAN 5
#define REG_LINK 6
#define REG_MULTI_SZ 7
#define REG_RESOURCE_LIST 8
#define REG_FULL_RESOURCE_DESCRIPTOR 9
#define REG_RESOURCE_REQUIREMENTS_LIST 10
#define REG_QWORD_LITTLE_ENDIAN 11
#endif

using RegistryDict = policy::RegistryDict;

namespace {

// Maximum PReg file size we're willing to accept.
const int64_t kMaxPRegFileSize = 1024 * 1024 * 16;
static_assert(kMaxPRegFileSize <= std::numeric_limits<ptrdiff_t>::max(),
              "Max PReg file size too large.");

// Maximum number of components in registry key names. This corresponds to the
// maximum nesting level of RegistryDict trees.
const size_t kMaxKeyNameComponents = 1024;

// Constants for PReg file delimiters.
const char16_t kDelimBracketOpen = u'[';
const char16_t kDelimBracketClose = u']';
const char16_t kDelimSemicolon = u';';

// Registry path separator.
const char16_t kRegistryPathSeparator[] = u"\\";

// Magic strings for the PReg value field to trigger special actions.
const char kActionTriggerPrefix[] = "**";
const char kActionTriggerDeleteValues[] = "deletevalues";
const char kActionTriggerDel[] = "del.";
const char kActionTriggerDelVals[] = "delvals";
const char kActionTriggerDeleteKeys[] = "deletekeys";
const char kActionTriggerSecureKey[] = "securekey";
const char kActionTriggerSoft[] = "soft";

// Returns the character at |cursor| and increments it, unless the end is here
// in which case -1 is returned. The calling code must guarantee that
// end - *cursor does not overflow ptrdiff_t.
int NextChar(const uint8_t** cursor, const uint8_t* end) {
  // Only read the character if a full char16_t is available.
  // This comparison makes sure no overflow can happen.
  if (*cursor >= end ||
      end - *cursor < static_cast<ptrdiff_t>(sizeof(char16_t)))
    return -1;

  int result = **cursor | (*(*cursor + 1) << 8);
  *cursor += sizeof(char16_t);
  return result;
}

// Reads a fixed-size field from a PReg file. The calling code must guarantee
// that both end - *cursor and size do not overflow ptrdiff_t.
bool ReadFieldBinary(const uint8_t** cursor,
                     const uint8_t* end,
                     uint32_t size,
                     uint8_t* data) {
  if (size == 0)
    return true;

  // Be careful to prevent possible overflows here (don't do *cursor + size).
  if (*cursor >= end || end - *cursor < static_cast<ptrdiff_t>(size))
    return false;
  const uint8_t* field_end = *cursor + size;
  std::copy(*cursor, field_end, data);
  *cursor = field_end;
  return true;
}

bool ReadField32(const uint8_t** cursor, const uint8_t* end, uint32_t* data) {
  uint32_t value = 0;
  if (!ReadFieldBinary(cursor, end, sizeof(uint32_t),
                       reinterpret_cast<uint8_t*>(&value))) {
    return false;
  }
  *data = base::ByteSwapToLE32(value);
  return true;
}

// Reads a string field from a file.
bool ReadFieldString(const uint8_t** cursor,
                     const uint8_t* end,
                     std::u16string* str) {
  int current = -1;
  while ((current = NextChar(cursor, end)) > 0x0000)
    *str += current;

  return current == L'\0';
}

// Converts the UTF16 |data| to an UTF8 string |value|. Returns false if the
// resulting UTF8 string contains invalid characters.
bool DecodePRegStringValue(const std::vector<uint8_t>& data,
                           std::string* value) {
  size_t len = data.size() / sizeof(char16_t);
  if (len <= 0) {
    value->clear();
    return true;
  }

  const char16_t* chars = reinterpret_cast<const char16_t*>(data.data());
  std::u16string utf16_str;
  std::transform(chars, chars + len - 1, std::back_inserter(utf16_str),
                 base::ByteSwapToLE16);
  // Note: UTF16ToUTF8() only checks whether all chars are valid code points,
  // but not whether they're valid characters. IsStringUTF8(), however, does.
  *value = base::UTF16ToUTF8(utf16_str);
  if (!base::IsStringUTF8(*value)) {
    LOG(ERROR) << "String '" << *value << "' is not a valid UTF8 string";
    value->clear();
    return false;
  }
  return true;
}

// Decodes a value from a PReg file given as a uint8_t vector.
bool DecodePRegValue(uint32_t type,
                     const std::vector<uint8_t>& data,
                     base::Value& value) {
  std::string data_utf8;
  switch (type) {
    case REG_SZ:
    case REG_EXPAND_SZ:
      if (!DecodePRegStringValue(data, &data_utf8))
        return false;
      value = base::Value(data_utf8);
      return true;
    case REG_DWORD_LITTLE_ENDIAN:
    case REG_DWORD_BIG_ENDIAN:
      if (data.size() == sizeof(uint32_t)) {
        uint32_t val = *reinterpret_cast<const uint32_t*>(data.data());
        if (type == REG_DWORD_BIG_ENDIAN)
          val = base::NetToHost32(val);
        else
          val = base::ByteSwapToLE32(val);
        value = base::Value(static_cast<int>(val));
        return true;
      } else {
        LOG(ERROR) << "Bad data size " << data.size();
      }
      break;
    case REG_NONE:
    case REG_LINK:
    case REG_MULTI_SZ:
    case REG_RESOURCE_LIST:
    case REG_FULL_RESOURCE_DESCRIPTOR:
    case REG_RESOURCE_REQUIREMENTS_LIST:
    case REG_QWORD_LITTLE_ENDIAN:
    default:
      LOG(ERROR) << "Unsupported registry data type " << type;
  }

  return false;
}

// Returns true if the registry key |key_name| belongs to the sub-tree specified
// by the key |root|.
bool KeyRootEquals(const std::u16string& key_name, const std::u16string& root) {
  if (root.empty())
    return true;

  if (!base::StartsWith(key_name, root, base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // Handle the case where |root| == "ABC" and |key_name| == "ABCDE\FG". This
  // should not be interpreted as a match.
  return key_name.length() == root.length() ||
         key_name.at(root.length()) == kRegistryPathSeparator[0];
}

// Adds |value| and |data| to |dict| or an appropriate sub-dictionary indicated
// by |key_name|. Creates sub-dictionaries if necessary. Also handles special
// action triggers, see |kActionTrigger*|, that can, for instance, remove an
// existing value.
void HandleRecord(const std::u16string& key_name,
                  const std::u16string& value,
                  uint32_t type,
                  const std::vector<uint8_t>& data,
                  RegistryDict* dict) {
  // Locate/create the dictionary to place the value in.
  std::vector<std::u16string> path;

  std::vector<base::StringPiece16> key_name_components =
      base::SplitStringPiece(key_name, kRegistryPathSeparator,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (key_name_components.size() > kMaxKeyNameComponents) {
    LOG(ERROR) << "Encountered a key which has more than "
               << kMaxKeyNameComponents << " components.";
    return;
  }
  for (const base::StringPiece16& key_name_component : key_name_components) {
    if (key_name_component.empty())
      continue;

    const std::string name = base::UTF16ToUTF8(key_name_component);
    RegistryDict* subdict = dict->GetKey(name);
    if (!subdict) {
      subdict = new RegistryDict();
      dict->SetKey(name, base::WrapUnique(subdict));
    }
    dict = subdict;
  }

  if (value.empty())
    return;

  std::string value_name(base::UTF16ToUTF8(value));
  if (!base::StartsWith(value_name, kActionTriggerPrefix,
                        base::CompareCase::SENSITIVE)) {
    base::Value preg_value;
    if (DecodePRegValue(type, data, preg_value))
      dict->SetValue(value_name, std::move(preg_value));
    return;
  }

  std::string data_utf8;
  std::string action_trigger(base::ToLowerASCII(
      value_name.substr(std::size(kActionTriggerPrefix) - 1)));
  if (action_trigger == kActionTriggerDeleteValues) {
    if (DecodePRegStringValue(data, &data_utf8)) {
      for (const std::string& value_str :
           base::SplitString(data_utf8, ";", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY))
        dict->RemoveValue(value_str);
    }
  } else if (base::StartsWith(action_trigger, kActionTriggerDeleteKeys,
                              base::CompareCase::SENSITIVE)) {
    if (DecodePRegStringValue(data, &data_utf8)) {
      for (const std::string& key :
           base::SplitString(data_utf8, ";", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY))
        dict->RemoveKey(key);
    }
  } else if (base::StartsWith(action_trigger, kActionTriggerDel,
                              base::CompareCase::SENSITIVE)) {
    dict->RemoveValue(value_name.substr(std::size(kActionTriggerPrefix) - 1 +
                                        std::size(kActionTriggerDel) - 1));
  } else if (base::StartsWith(action_trigger, kActionTriggerDelVals,
                              base::CompareCase::SENSITIVE)) {
    // Delete all values.
    dict->ClearValues();
  } else if (base::StartsWith(action_trigger, kActionTriggerSecureKey,
                              base::CompareCase::SENSITIVE) ||
             base::StartsWith(action_trigger, kActionTriggerSoft,
                              base::CompareCase::SENSITIVE)) {
    // Doesn't affect values.
  } else {
    LOG(ERROR) << "Bad action trigger " << value_name;
  }
}

}  // namespace

namespace policy {
namespace preg_parser {

const char kPRegFileHeader[8] = {'P',    'R',    'e',    'g',
                                 '\x01', '\x00', '\x00', '\x00'};

bool ReadFile(const base::FilePath& file_path,
              const std::u16string& root,
              RegistryDict* dict,
              PolicyLoadStatusSampler* status) {
  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(file_path) || !mapped_file.IsValid()) {
    PLOG(ERROR) << "Failed to map " << file_path.value();
    status->Add(POLICY_LOAD_STATUS_READ_ERROR);
    return false;
  }

  return ReadDataInternal(
      mapped_file.data(), mapped_file.length(), root, dict, status,
      base::StringPrintf("file '%" PRFilePath "'", file_path.value().c_str()));
}

POLICY_EXPORT bool ReadDataInternal(const uint8_t* preg_data,
                                    size_t preg_data_size,
                                    const std::u16string& root,
                                    RegistryDict* dict,
                                    PolicyLoadStatusSampler* status,
                                    const std::string& debug_name) {
  DCHECK(status);
  DCHECK(root.empty() || root.back() != kRegistryPathSeparator[0]);

  // Check data size.
  if (preg_data_size > kMaxPRegFileSize) {
    LOG(ERROR) << "PReg " << debug_name << " too large: " << preg_data_size;
    status->Add(POLICY_LOAD_STATUS_TOO_BIG);
    return false;
  }

  // Check the header.
  const int kHeaderSize = std::size(kPRegFileHeader);
  if (!preg_data || preg_data_size < kHeaderSize ||
      memcmp(kPRegFileHeader, preg_data, kHeaderSize) != 0) {
    LOG(ERROR) << "Bad PReg " << debug_name;
    status->Add(POLICY_LOAD_STATUS_PARSE_ERROR);
    return false;
  }

  // Parse data, which is expected to be UCS-2 and little-endian. The latter I
  // couldn't find documentation on, but the example I saw were all
  // little-endian. It'd be interesting to check on big-endian hardware.
  const uint8_t* cursor = preg_data + kHeaderSize;
  const uint8_t* end = preg_data + preg_data_size;
  while (true) {
    if (cursor == end)
      return true;

    if (NextChar(&cursor, end) != kDelimBracketOpen)
      break;

    // Read the record fields.
    std::u16string key_name;
    std::u16string value;
    uint32_t type = 0;
    uint32_t size = 0;
    std::vector<uint8_t> data;

    if (!ReadFieldString(&cursor, end, &key_name))
      break;

    int current = NextChar(&cursor, end);
    if (current == kDelimSemicolon) {
      if (!ReadFieldString(&cursor, end, &value))
        break;
      current = NextChar(&cursor, end);
    }

    if (current == kDelimSemicolon) {
      if (!ReadField32(&cursor, end, &type))
        break;
      current = NextChar(&cursor, end);
    }

    if (current == kDelimSemicolon) {
      if (!ReadField32(&cursor, end, &size))
        break;
      current = NextChar(&cursor, end);
    }

    if (current == kDelimSemicolon) {
      if (size > kMaxPRegFileSize)
        break;
      data.resize(size);
      if (!ReadFieldBinary(&cursor, end, size, data.data()))
        break;
      current = NextChar(&cursor, end);
    }

    if (current != kDelimBracketClose)
      break;

    // Process the record if it is within the |root| subtree.
    if (KeyRootEquals(key_name, root))
      HandleRecord(key_name.substr(root.size()), value, type, data, dict);
  }

  LOG(ERROR) << "Error parsing PReg " << debug_name << " at offset "
             << (reinterpret_cast<const uint8_t*>(cursor - 1) - preg_data);
  status->Add(POLICY_LOAD_STATUS_PARSE_ERROR);
  return false;
}

}  // namespace preg_parser
}  // namespace policy
