// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/config_dir_policy_loader.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

namespace {

// Subdirectories that contain the mandatory and recommended policies.
constexpr base::FilePath::CharType kMandatoryConfigDir[] =
    FILE_PATH_LITERAL("managed");
constexpr base::FilePath::CharType kRecommendedConfigDir[] =
    FILE_PATH_LITERAL("recommended");

PolicyLoadStatus JsonErrorToPolicyLoadStatus(int status) {
  switch (status) {
    case JSONFileValueDeserializer::JSON_ACCESS_DENIED:
    case JSONFileValueDeserializer::JSON_CANNOT_READ_FILE:
    case JSONFileValueDeserializer::JSON_FILE_LOCKED:
      return POLICY_LOAD_STATUS_READ_ERROR;
    case JSONFileValueDeserializer::JSON_NO_SUCH_FILE:
      return POLICY_LOAD_STATUS_MISSING;
    case base::JSONReader::JSON_INVALID_ESCAPE:
    case base::JSONReader::JSON_SYNTAX_ERROR:
    case base::JSONReader::JSON_UNEXPECTED_TOKEN:
    case base::JSONReader::JSON_TRAILING_COMMA:
    case base::JSONReader::JSON_TOO_MUCH_NESTING:
    case base::JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT:
    case base::JSONReader::JSON_UNSUPPORTED_ENCODING:
    case base::JSONReader::JSON_UNQUOTED_DICTIONARY_KEY:
      return POLICY_LOAD_STATUS_PARSE_ERROR;
    case base::JSONReader::JSON_NO_ERROR:
      NOTREACHED();
      return POLICY_LOAD_STATUS_STARTED;
  }
  NOTREACHED() << "Invalid status " << status;
  return POLICY_LOAD_STATUS_PARSE_ERROR;
}

}  // namespace

ConfigDirPolicyLoader::ConfigDirPolicyLoader(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& config_dir,
    PolicyScope scope)
    : AsyncPolicyLoader(task_runner),
      task_runner_(task_runner),
      config_dir_(config_dir),
      scope_(scope) {}

ConfigDirPolicyLoader::~ConfigDirPolicyLoader() {}

void ConfigDirPolicyLoader::InitOnBackgroundThread() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::FilePathWatcher::Callback callback =
      base::Bind(&ConfigDirPolicyLoader::OnFileUpdated, base::Unretained(this));
  mandatory_watcher_.Watch(config_dir_.Append(kMandatoryConfigDir), false,
                           callback);
  recommended_watcher_.Watch(config_dir_.Append(kRecommendedConfigDir), false,
                             callback);
}

std::unique_ptr<PolicyBundle> ConfigDirPolicyLoader::Load() {
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
  LoadFromPath(config_dir_.Append(kMandatoryConfigDir),
               POLICY_LEVEL_MANDATORY,
               bundle.get());
  LoadFromPath(config_dir_.Append(kRecommendedConfigDir),
               POLICY_LEVEL_RECOMMENDED,
               bundle.get());
  return bundle;
}

base::Time ConfigDirPolicyLoader::LastModificationTime() {
  static constexpr const base::FilePath::CharType* kConfigDirSuffixes[] = {
      kMandatoryConfigDir, kRecommendedConfigDir,
  };

  base::Time last_modification = base::Time();
  base::File::Info info;

  for (size_t i = 0; i < base::size(kConfigDirSuffixes); ++i) {
    base::FilePath path(config_dir_.Append(kConfigDirSuffixes[i]));

    // Skip if the file doesn't exist, or it isn't a directory.
    if (!base::GetFileInfo(path, &info) || !info.is_directory)
      continue;

    // Enumerate the files and find the most recent modification timestamp.
    base::FileEnumerator file_enumerator(path, false,
                                         base::FileEnumerator::FILES);
    for (base::FilePath config_file = file_enumerator.Next();
         !config_file.empty();
         config_file = file_enumerator.Next()) {
      if (base::GetFileInfo(config_file, &info) && !info.is_directory)
        last_modification = std::max(last_modification, info.last_modified);
    }
  }

  return last_modification;
}

void ConfigDirPolicyLoader::LoadFromPath(const base::FilePath& path,
                                         PolicyLevel level,
                                         PolicyBundle* bundle) {
  // Enumerate the files and sort them lexicographically.
  std::set<base::FilePath> files;
  base::FileEnumerator file_enumerator(path, false,
                                       base::FileEnumerator::FILES);
  for (base::FilePath config_file_path = file_enumerator.Next();
       !config_file_path.empty(); config_file_path = file_enumerator.Next())
    files.insert(config_file_path);

  PolicyLoadStatusUmaReporter status;
  if (files.empty()) {
    status.Add(POLICY_LOAD_STATUS_NO_POLICY);
    return;
  }

  // Start with an empty dictionary and merge the files' contents.
  // The files are processed in reverse order because |MergeFrom| gives priority
  // to existing keys, but the ConfigDirPolicyProvider gives priority to the
  // last file in lexicographic order.
  for (auto config_file_iter = files.rbegin(); config_file_iter != files.rend();
       ++config_file_iter) {
    JSONFileValueDeserializer deserializer(*config_file_iter,
                                           base::JSON_ALLOW_TRAILING_COMMAS);
    int error_code = 0;
    std::string error_msg;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(&error_code, &error_msg);
    if (!value) {
      LOG(WARNING) << "Failed to read configuration file "
                   << config_file_iter->value() << ": " << error_msg;
      status.Add(JsonErrorToPolicyLoadStatus(error_code));
      continue;
    }
    base::DictionaryValue* dictionary_value = nullptr;
    if (!value->GetAsDictionary(&dictionary_value)) {
      LOG(WARNING) << "Expected JSON dictionary in configuration file "
                   << config_file_iter->value();
      status.Add(POLICY_LOAD_STATUS_PARSE_ERROR);
      continue;
    }

    // Detach the "3rdparty" node.
    std::unique_ptr<base::Value> third_party;
    if (dictionary_value->Remove("3rdparty", &third_party)) {
      Merge3rdPartyPolicy(third_party.get(), level, bundle,
                          /*signin_profile=*/true);
      Merge3rdPartyPolicy(third_party.get(), level, bundle,
                          /*signin_profile=*/false);
    }

    // Add chrome policy.
    PolicyMap policy_map;
    policy_map.LoadFrom(dictionary_value, level, scope_,
                        POLICY_SOURCE_PLATFORM);
    bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .MergeFrom(policy_map);
  }
}

void ConfigDirPolicyLoader::Merge3rdPartyPolicy(const base::Value* policies,
                                                PolicyLevel level,
                                                PolicyBundle* bundle,
                                                bool signin_profile) {
  // The first-level entries in |policies| are PolicyDomains. The second-level
  // entries are component IDs, and the third-level entries are the policies
  // for that domain/component namespace.

  const base::DictionaryValue* domains_dictionary;
  if (!policies->GetAsDictionary(&domains_dictionary)) {
    LOG(WARNING) << "3rdparty value is not a dictionary!";
    return;
  }

  // Helper to lookup a domain given its string name.
  std::map<std::string, PolicyDomain> supported_domains;
  supported_domains["extensions"] = signin_profile
                                        ? POLICY_DOMAIN_SIGNIN_EXTENSIONS
                                        : POLICY_DOMAIN_EXTENSIONS;

  for (base::DictionaryValue::Iterator domains_it(*domains_dictionary);
       !domains_it.IsAtEnd(); domains_it.Advance()) {
    if (!base::Contains(supported_domains, domains_it.key())) {
      LOG(WARNING) << "Unsupported 3rd party policy domain: "
                   << domains_it.key();
      continue;
    }

    const base::DictionaryValue* components_dictionary;
    if (!domains_it.value().GetAsDictionary(&components_dictionary)) {
      LOG(WARNING) << "3rdparty/" << domains_it.key()
                   << " value is not a dictionary!";
      continue;
    }

    PolicyDomain domain = supported_domains[domains_it.key()];
    for (base::DictionaryValue::Iterator components_it(*components_dictionary);
         !components_it.IsAtEnd(); components_it.Advance()) {
      const base::DictionaryValue* policy_dictionary;
      if (!components_it.value().GetAsDictionary(&policy_dictionary)) {
        LOG(WARNING) << "3rdparty/" << domains_it.key() << "/"
                     << components_it.key() << " value is not a dictionary!";
        continue;
      }

      PolicyMap policy;
      policy.LoadFrom(policy_dictionary, level, scope_, POLICY_SOURCE_PLATFORM);
      bundle->Get(PolicyNamespace(domain, components_it.key()))
          .MergeFrom(policy);
    }
  }
}

void ConfigDirPolicyLoader::OnFileUpdated(const base::FilePath& path,
                                          bool error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!error)
    Reload(false);
}

}  // namespace policy
