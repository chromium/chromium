// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"

namespace chrome_cleaner {

typedef base::RefCountedData<base::Value> RefValue;
typedef uint32_t ContentType;

constexpr int64_t kParseAttemptTimeoutMilliseconds = 10000;

// A registry key that holds some form of policy for |extension_id|.
struct ExtensionPolicyRegistryEntry {
  std::wstring extension_id;
  HKEY hkey;
  std::wstring path;
  std::wstring name;
  ContentType content_type;
  scoped_refptr<RefValue> json;

  ExtensionPolicyRegistryEntry(const std::wstring& extension_id,
                               HKEY hkey,
                               const std::wstring& path,
                               const std::wstring& name,
                               ContentType content_type,
                               scoped_refptr<RefValue>);

  ExtensionPolicyRegistryEntry(const ExtensionPolicyRegistryEntry&) = delete;
  ExtensionPolicyRegistryEntry& operator=(const ExtensionPolicyRegistryEntry&) =
      delete;

  ExtensionPolicyRegistryEntry(ExtensionPolicyRegistryEntry&&);
  ExtensionPolicyRegistryEntry& operator=(ExtensionPolicyRegistryEntry&&);

  ~ExtensionPolicyRegistryEntry();
};

// A file that holds some form of policy for |extension_id|.
struct ExtensionPolicyFile {
  std::wstring extension_id;
  base::FilePath path;
  scoped_refptr<RefValue> json;

  ExtensionPolicyFile(const std::wstring& extension_id,
                      const base::FilePath& path,
                      scoped_refptr<RefValue> json);
  ExtensionPolicyFile(ExtensionPolicyFile&&);

  ExtensionPolicyFile(const ExtensionPolicyFile&) = delete;
  ExtensionPolicyFile& operator=(const ExtensionPolicyFile&) = delete;

  ExtensionPolicyFile& operator=(ExtensionPolicyFile&&);

  ~ExtensionPolicyFile();
};

// Find all extension forcelist registry policies and append to |policies|.
void GetExtensionForcelistRegistryPolicies(
    std::vector<ExtensionPolicyRegistryEntry>* policies);

// Find non-whitelisted extension IDs in external_extensions.json files, which
// are extensions that will be installed by default on new user profiles. Using
// the input |json_parser| to parse JSON, append found extensions to |policies|.
// Signals |done| when all parse tasks have finished.
void GetNonWhitelistedDefaultExtensions(
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyFile>* policies,
    base::WaitableEvent* done);

// Find all extensions whose enterprise policy settings contain
// "installation_mode":"force_installed" and append them to |policies|. Uses the
// input |json_parser| to parse JSON, and signals |done| when all parse tasks
// have finished.
void GetExtensionSettingsForceInstalledExtensions(
    JsonParserAPI* json_parser,
    std::vector<ExtensionPolicyRegistryEntry>* policies,
    base::WaitableEvent* done);

// Find master preferences extensions, which are installed to new user profiles,
// and append them to |policies|. Uses the input |json_parser| to parse JSON,
// and signals |done| when all parse tasks have finished.
void GetMasterPreferencesExtensions(JsonParserAPI* json_parser,
                                    std::vector<ExtensionPolicyFile>* policies,
                                    base::WaitableEvent* done);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
