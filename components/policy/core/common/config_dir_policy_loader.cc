// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/config_dir_policy_loader.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

namespace {

// Subdirectories that contain the mandatory and recommended policies.
constexpr base::FilePath::CharType kMandatoryConfigDir[] =
    FILE_PATH_LITERAL("managed");
constexpr base::FilePath::CharType kRecommendedConfigDir[] =
    FILE_PATH_LITERAL("recommended");

}  // namespace

ConfigDirPolicyLoader::ConfigDirPolicyLoader(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& config_dir,
    PolicyScope scope)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/true),
      task_runner_(task_runner),
      config_dir_(config_dir),
      scope_(scope) {}

ConfigDirPolicyLoader::~ConfigDirPolicyLoader() = default;

void ConfigDirPolicyLoader::InitOnBackgroundThread() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::FilePathWatcher::Callback callback = base::BindRepeating(
      &ConfigDirPolicyLoader::OnFileUpdated, base::Unretained(this));
  mandatory_watcher_.Watch(config_dir_.Append(kMandatoryConfigDir),
                           base::FilePathWatcher::Type::kNonRecursive,
                           callback);
  recommended_watcher_.Watch(config_dir_.Append(kRecommendedConfigDir),
                             base::FilePathWatcher::Type::kNonRecursive,
                             callback);
}

PolicyBundle ConfigDirPolicyLoader::Load() {
  PolicyBundle bundle;
  LoadFromPath(config_dir_.Append(kMandatoryConfigDir), POLICY_LEVEL_MANDATORY,
               &bundle);
  LoadFromPath(config_dir_.Append(kRecommendedConfigDir),
               POLICY_LEVEL_RECOMMENDED, &bundle);
  return bundle;
}

base::Time ConfigDirPolicyLoader::LastModificationTime() {
  static constexpr const base::FilePath::CharType* kConfigDirSuffixes[] = {
      kMandatoryConfigDir, kRecommendedConfigDir,
  };

  base::Time last_modification = base::Time();
  base::File::Info info;

  for (size_t i = 0; i < std::size(kConfigDirSuffixes); ++i) {
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
  std::string policy_level =
      level == POLICY_LEVEL_MANDATORY ? "mandatory" : "recommended";
  base::FileEnumerator file_enumerator(path, false,
                                       base::FileEnumerator::FILES);
  for (base::FilePath config_file_path = file_enumerator.Next();
       !config_file_path.empty(); config_file_path = file_enumerator.Next()) {
    files.insert(config_file_path);
    VLOG_POLICY(1, POLICY_FETCHING)
        << "Found " << policy_level << " policy file: " << config_file_path;
  }

  if (files.empty()) {
    VLOG_POLICY(1, POLICY_FETCHING)
        << "Skipping " << policy_level
        << " platform policies because no policy file was found at: " << path;
    return;
  }

  // Start with an empty dictionary and merge the files' contents.
  // The files are processed in reverse order because |MergeFrom| gives priority
  // to existing keys, but the ConfigDirPolicyProvider gives priority to the
  // last file in lexicographic order.
  for (const base::FilePath& config_file : base::Reversed(files)) {
    JSONFileValueDeserializer deserializer(
        config_file, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                         base::JSON_ALLOW_TRAILING_COMMAS);
    std::string error_msg;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(nullptr, &error_msg);
    if (!value) {
      SYSLOG(WARNING) << "Failed to read configuration file "
                      << config_file.value() << ": " << error_msg;
      continue;
    }
    base::Value::Dict* dictionary_value = value->GetIfDict();
    if (!dictionary_value) {
      SYSLOG(WARNING) << "Expected JSON dictionary in configuration file "
                      << config_file.value();
      continue;
    }

    // Detach the "3rdparty" node.
    std::optional<base::Value> third_party =
        dictionary_value->Extract("3rdparty");
    if (third_party.has_value()) {
      Merge3rdPartyPolicy(&*third_party, level, bundle,
                          /*signin_profile=*/true);
      Merge3rdPartyPolicy(&*third_party, level, bundle,
                          /*signin_profile=*/false);
    }

    // Add chrome policy.
    PolicyMap policy_map;
    policy_map.LoadFrom(*dictionary_value, level, scope_,
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

  const base::Value::Dict* domains_dictionary = policies->GetIfDict();
  if (!domains_dictionary) {
    SYSLOG(WARNING) << "3rdparty value is not a dictionary!";
    return;
  }

  // Helper to lookup a domain given its string name.
  std::map<std::string, PolicyDomain> supported_domains;
  supported_domains["extensions"] = signin_profile
                                        ? POLICY_DOMAIN_SIGNIN_EXTENSIONS
                                        : POLICY_DOMAIN_EXTENSIONS;

  for (auto domains_it : *domains_dictionary) {
    if (!base::Contains(supported_domains, domains_it.first)) {
      SYSLOG(WARNING) << "Unsupported 3rd party policy domain: "
                      << domains_it.first;
      continue;
    }

    const base::Value::Dict* components_dictionary =
        domains_it.second.GetIfDict();
    if (!components_dictionary) {
      SYSLOG(WARNING) << "3rdparty/" << domains_it.first
                      << " value is not a dictionary!";
      continue;
    }

    PolicyDomain domain = supported_domains[domains_it.first];
    for (auto components_it : *components_dictionary) {
      const base::Value::Dict* policy_dictionary =
          components_it.second.GetIfDict();
      if (!policy_dictionary) {
        SYSLOG(WARNING) << "3rdparty/" << domains_it.first << "/"
                        << components_it.first << " value is not a dictionary!";
        continue;
      }

      PolicyMap policy;
      policy.LoadFrom(*policy_dictionary, level, scope_,
                      POLICY_SOURCE_PLATFORM);
      bundle->Get(PolicyNamespace(domain, components_it.first))
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
