// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/initial_preferences.h"

#include <stddef.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "components/variations/pref_names.h"
#include "rlz/buildflags/buildflags.h"

namespace {

const char kFirstRunTabs[] = "first_run_tabs";

base::LazyInstance<installer::InitialPreferences>::DestructorAtExit
    g_initial_preferences = LAZY_INSTANCE_INITIALIZER;

bool GetURLFromValue(const base::Value* in_value, std::string* out_value) {
  return in_value && out_value && in_value->GetAsString(out_value);
}

std::vector<std::string> GetNamedList(const char* name,
                                      const base::DictionaryValue* prefs) {
  std::vector<std::string> list;
  if (!prefs)
    return list;

  const base::ListValue* value_list = nullptr;
  if (!prefs->GetList(name, &value_list))
    return list;

  list.reserve(value_list->GetSize());
  for (size_t i = 0; i < value_list->GetSize(); ++i) {
    const base::Value* entry;
    std::string url_entry;
    if (!value_list->Get(i, &entry) || !GetURLFromValue(entry, &url_entry)) {
      NOTREACHED();
      break;
    }
    list.push_back(url_entry);
  }
  return list;
}

base::DictionaryValue* ParseDistributionPreferences(
    const std::string& json_data) {
  JSONStringValueDeserializer json(json_data);
  std::string error;
  std::unique_ptr<base::Value> root(json.Deserialize(nullptr, &error));
  if (!root.get()) {
    LOG(WARNING) << "Failed to parse initial prefs file: " << error;
    return nullptr;
  }
  if (!root->is_dict()) {
    LOG(WARNING) << "Failed to parse initial prefs file: "
                 << "Root item must be a dictionary.";
    return nullptr;
  }
  return static_cast<base::DictionaryValue*>(root.release());
}

}  // namespace

namespace installer {

InitialPreferences::InitialPreferences() {
  InitializeFromCommandLine(*base::CommandLine::ForCurrentProcess());
}

InitialPreferences::InitialPreferences(const base::CommandLine& cmd_line) {
  InitializeFromCommandLine(cmd_line);
}

InitialPreferences::InitialPreferences(const base::FilePath& prefs_path) {
  InitializeFromFilePath(prefs_path);
}

InitialPreferences::InitialPreferences(const std::string& prefs) {
  InitializeFromString(prefs);
}

InitialPreferences::InitialPreferences(const base::DictionaryValue& prefs)
    : initial_dictionary_(prefs.CreateDeepCopy()) {
  // Cache a pointer to the distribution dictionary.
  initial_dictionary_->GetDictionary(
      installer::initial_preferences::kDistroDict, &distribution_);

  EnforceLegacyPreferences();
}

InitialPreferences::~InitialPreferences() = default;

void InitialPreferences::InitializeFromCommandLine(
    const base::CommandLine& cmd_line) {
#if defined(OS_WIN)
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    base::FilePath prefs_path(
        cmd_line.GetSwitchValuePath(installer::switches::kInstallerData));
    InitializeFromFilePath(prefs_path);
  } else {
    initial_dictionary_.reset(new base::DictionaryValue());
  }

  DCHECK(initial_dictionary_.get());

  // A simple map from command line switches to equivalent switches in the
  // distribution dictionary.  Currently all switches added will be set to
  // 'true'.
  static const struct CmdLineSwitchToDistributionSwitch {
    const char* cmd_line_switch;
    const char* distribution_switch;
  } translate_switches[] = {
      {installer::switches::kAllowDowngrade,
       installer::initial_preferences::kAllowDowngrade},
      {installer::switches::kDisableLogging,
       installer::initial_preferences::kDisableLogging},
      {installer::switches::kMsi, installer::initial_preferences::kMsi},
      {installer::switches::kDoNotRegisterForUpdateLaunch,
       installer::initial_preferences::kDoNotRegisterForUpdateLaunch},
      {installer::switches::kDoNotLaunchChrome,
       installer::initial_preferences::kDoNotLaunchChrome},
      {installer::switches::kMakeChromeDefault,
       installer::initial_preferences::kMakeChromeDefault},
      {installer::switches::kSystemLevel,
       installer::initial_preferences::kSystemLevel},
      {installer::switches::kVerboseLogging,
       installer::initial_preferences::kVerboseLogging},
  };

  std::string name(installer::initial_preferences::kDistroDict);
  for (size_t i = 0; i < base::size(translate_switches); ++i) {
    if (cmd_line.HasSwitch(translate_switches[i].cmd_line_switch)) {
      name.assign(installer::initial_preferences::kDistroDict);
      name.append(".").append(translate_switches[i].distribution_switch);
      initial_dictionary_->SetBoolean(name, true);
    }
  }

  // See if the log file path was specified on the command line.
  std::wstring str_value(
      cmd_line.GetSwitchValueNative(installer::switches::kLogFile));
  if (!str_value.empty()) {
    name.assign(installer::initial_preferences::kDistroDict);
    name.append(".").append(installer::initial_preferences::kLogFile);
    initial_dictionary_->SetString(name, base::WideToUTF8(str_value));
  }

  // Handle the special case of --system-level being implied by the presence of
  // the kGoogleUpdateIsMachineEnvVar environment variable.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env) {
    std::string is_machine_var;
    env->GetVar(env_vars::kGoogleUpdateIsMachineEnvVar, &is_machine_var);
    if (is_machine_var == "1") {
      VLOG(1) << "Taking system-level from environment.";
      name.assign(installer::initial_preferences::kDistroDict);
      name.append(".").append(installer::initial_preferences::kSystemLevel);
      initial_dictionary_->SetBoolean(name, true);
    }
  }

  // Cache a pointer to the distribution dictionary. Ignore errors if any.
  initial_dictionary_->GetDictionary(
      installer::initial_preferences::kDistroDict, &distribution_);
#endif
}

void InitialPreferences::InitializeFromFilePath(
    const base::FilePath& prefs_path) {
  std::string json_data;
  // Failure to read the file is ignored as |json_data| will be the empty string
  // and the remainder of this InitialPreferences object should still be
  // initialized as best as possible.
  if (base::PathExists(prefs_path) &&
      !base::ReadFileToString(prefs_path, &json_data)) {
    LOG(ERROR) << "Failed to read preferences from " << prefs_path.value();
  }
  if (InitializeFromString(json_data))
    preferences_read_from_file_ = true;
}

bool InitialPreferences::InitializeFromString(const std::string& json_data) {
  if (!json_data.empty())
    initial_dictionary_.reset(ParseDistributionPreferences(json_data));

  bool data_is_valid = true;
  if (!initial_dictionary_.get()) {
    initial_dictionary_ = std::make_unique<base::DictionaryValue>();
    data_is_valid = false;
  } else {
    // Cache a pointer to the distribution dictionary.
    initial_dictionary_->GetDictionary(
        installer::initial_preferences::kDistroDict, &distribution_);
  }

  EnforceLegacyPreferences();
  return data_is_valid;
}

void InitialPreferences::EnforceLegacyPreferences() {
  // Boolean. This is a legacy preference and should no longer be used; it is
  // kept around so that old master_preferences which specify
  // "create_all_shortcuts":false still enforce the new
  // "do_not_create_(desktop|quick_launch)_shortcut" preferences. Setting this
  // to true no longer has any impact.
  static constexpr char kCreateAllShortcuts[] = "create_all_shortcuts";

  // If create_all_shortcuts was explicitly set to false, set
  // do_not_create_(desktop|quick_launch)_shortcut to true.
  bool create_all_shortcuts = true;
  GetBool(kCreateAllShortcuts, &create_all_shortcuts);
  if (!create_all_shortcuts) {
    distribution_->SetBoolean(
        installer::initial_preferences::kDoNotCreateDesktopShortcut, true);
    distribution_->SetBoolean(
        installer::initial_preferences::kDoNotCreateQuickLaunchShortcut, true);
  }

  // Deprecated boolean import initial preferences now mapped to their
  // duplicates in prefs::.
  static constexpr char kDistroImportHistoryPref[] = "import_history";
  static constexpr char kDistroImportHomePagePref[] = "import_home_page";
  static constexpr char kDistroImportSearchPref[] = "import_search_engine";
  static constexpr char kDistroImportBookmarksPref[] = "import_bookmarks";

  static constexpr struct {
    const char* old_distro_pref_path;
    const char* modern_pref_path;
  } kLegacyDistroImportPrefMappings[] = {
      {kDistroImportBookmarksPref, prefs::kImportBookmarks},
      {kDistroImportHistoryPref, prefs::kImportHistory},
      {kDistroImportHomePagePref, prefs::kImportHomepage},
      {kDistroImportSearchPref, prefs::kImportSearchEngine},
  };

  for (const auto& mapping : kLegacyDistroImportPrefMappings) {
    bool value = false;
    if (GetBool(mapping.old_distro_pref_path, &value))
      initial_dictionary_->SetBoolean(mapping.modern_pref_path, value);
  }

#if BUILDFLAG(ENABLE_RLZ)
  // Map the RLZ ping delay shipped in the distribution dictionary into real
  // prefs.
  static constexpr char kDistroPingDelay[] = "ping_delay";
  int rlz_ping_delay = 0;
  if (GetInt(kDistroPingDelay, &rlz_ping_delay))
    initial_dictionary_->SetInteger(prefs::kRlzPingDelaySeconds,
                                    rlz_ping_delay);
#endif  // BUILDFLAG(ENABLE_RLZ)
}

bool InitialPreferences::GetBool(const std::string& name, bool* value) const {
  bool ret = false;
  if (distribution_)
    ret = distribution_->GetBoolean(name, value);
  return ret;
}

bool InitialPreferences::GetInt(const std::string& name, int* value) const {
  bool ret = false;
  if (distribution_)
    ret = distribution_->GetInteger(name, value);
  return ret;
}

bool InitialPreferences::GetString(const std::string& name,
                                   std::string* value) const {
  bool ret = false;
  if (distribution_)
    ret = (distribution_->GetString(name, value) && !value->empty());
  return ret;
}

bool InitialPreferences::GetPath(const std::string& name,
                                 base::FilePath* value) const {
  std::string string_value;
  if (!GetString(name, &string_value))
    return false;
  *value = base::FilePath::FromUTF8Unsafe(string_value);
  return true;
}

std::vector<std::string> InitialPreferences::GetFirstRunTabs() const {
  return GetNamedList(kFirstRunTabs, initial_dictionary_.get());
}

bool InitialPreferences::GetExtensionsBlock(
    base::DictionaryValue** extensions) const {
  return initial_dictionary_->GetDictionary(
      initial_preferences::kExtensionsBlock, extensions);
}

std::string InitialPreferences::GetCompressedVariationsSeed() const {
  return ExtractPrefString(variations::prefs::kVariationsCompressedSeed);
}

std::string InitialPreferences::GetVariationsSeedSignature() const {
  return ExtractPrefString(variations::prefs::kVariationsSeedSignature);
}

std::string InitialPreferences::ExtractPrefString(
    const std::string& name) const {
  std::string result;
  std::unique_ptr<base::Value> pref_value;
  if (initial_dictionary_->Remove(name, &pref_value)) {
    if (!pref_value->GetAsString(&result))
      NOTREACHED();
  }
  return result;
}

// static
const InitialPreferences& InitialPreferences::ForCurrentProcess() {
  return g_initial_preferences.Get();
}

}  // namespace installer
