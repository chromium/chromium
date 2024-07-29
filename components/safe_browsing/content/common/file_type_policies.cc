// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies.h"

#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/grit/components_resources.h"
#include "components/safe_browsing/content/common/file_type_policies_policy_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace policy {

using ExtensionToPolicyMap = std::map<std::string, const DownloadFileType>;

ExtensionToPolicyMap& GetExtensionToPolicyMap() {
  static base::NoDestructor<ExtensionToPolicyMap> ext_map;
  return *ext_map;
}

const DownloadFileType& GetOrCreatePolicyForExtensionOverrideNotDangerous(
    const std::string& extension,
    const DownloadFileType& base_policy) {
  ExtensionToPolicyMap& ext_map = GetExtensionToPolicyMap();
  auto it = ext_map.find(extension);
  // Cache hit, don't bother recreating a duplicate policy structure.
  if (it != ext_map.end())
    return it->second;

  // Create copy of existing settings.
  const DownloadFileType::PlatformSettings& base_settings =
      base_policy.platform_settings(0);
  DownloadFileType override_policy = base_policy;

  // Override only the danger level.
  override_policy.clear_platform_settings();
  auto* override_settings = override_policy.add_platform_settings();
  *override_settings = base_settings;
  override_settings->set_danger_level(DownloadFileType::NOT_DANGEROUS);

  // Add element to our map so we can hand out a valid reference.
  return ext_map.insert({extension, override_policy}).first->second;
}

}  // namespace policy

using base::AutoLock;

// Our Singleton needs to populate itself when first constructed.  This
// is left out of the constructor to make testing simpler. This
// singleton needs to be leaky so that archive analyzers can access file
// type policies from a helper thread. FileTypePolicies must not do
// nontrivial work in the destructor.
struct FileTypePoliciesSingletonTrait
    : public base::LeakySingletonTraits<FileTypePolicies> {
  static FileTypePolicies* New() {
    FileTypePolicies* instance = new FileTypePolicies();
    instance->PopulateFromResourceBundle();
    return instance;
  }
};

// --- FileTypePolicies methods ---

// static
FileTypePolicies* FileTypePolicies::GetInstance() {
  return base::Singleton<FileTypePolicies,
                         FileTypePoliciesSingletonTrait>::get();
}

FileTypePolicies::FileTypePolicies() {
  // Setup a file-type policy to use if the ResourceBundle is unreadable.
  // This should normally never be used.
  last_resort_default_.set_uma_value(-1l);
  last_resort_default_.set_ping_setting(DownloadFileType::NO_PING);
  auto* settings = last_resort_default_.add_platform_settings();
  settings->set_danger_level(DownloadFileType::ALLOW_ON_USER_GESTURE);
  settings->set_auto_open_hint(DownloadFileType::DISALLOW_AUTO_OPEN);
}

// Since FileTypePolicies is a leaky singleton, the destructor will
// never run.
FileTypePolicies::~FileTypePolicies() = default;

std::string FileTypePolicies::ReadResourceBundle() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  return bundle.LoadDataResourceString(IDR_DOWNLOAD_FILE_TYPES_PB);
}

void FileTypePolicies::RecordUpdateMetrics(UpdateResult result,
                                           const std::string& src_name) {
  lock_.AssertAcquired();
  // src_name should be "ResourceBundle" or "DynamicUpdate".
  base::UmaHistogramSparse("SafeBrowsing.FileTypeUpdate." + src_name + "Result",
                           static_cast<unsigned int>(result));

  if (result == UpdateResult::SUCCESS) {
    base::UmaHistogramSparse(
        "SafeBrowsing.FileTypeUpdate." + src_name + "Version",
        config_->version_id());
  }
}

void FileTypePolicies::PopulateFromResourceBundle() {
  AutoLock lock(lock_);
  std::string binary_pb = ReadResourceBundle();
  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, "ResourceBundle");
}

void FileTypePolicies::PopulateFromDynamicUpdate(const std::string& binary_pb) {
  AutoLock lock(lock_);
  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, "DynamicUpdate");
}

FileTypePolicies::UpdateResult FileTypePolicies::PopulateFromBinaryPb(
    const std::string& binary_pb) {
  lock_.AssertAcquired();

  // Parse the proto and do some validation on it.
  if (binary_pb.empty())
    return UpdateResult::FAILED_EMPTY;

  std::unique_ptr<DownloadFileTypeConfig> new_config(
      new DownloadFileTypeConfig);
  if (!new_config->ParseFromString(binary_pb))
    return UpdateResult::FAILED_PROTO_PARSE;

  // Need at least a default setting.
  if (new_config->default_file_type().platform_settings().size() == 0)
    return UpdateResult::FAILED_DEFAULT_SETTING_SET;

  // Every file type should have exactly one setting, pre-filtered for this
  // platform.
  for (const auto& file_type : new_config->file_types()) {
    if (file_type.platform_settings().size() != 1)
      return UpdateResult::FAILED_WRONG_SETTINGS_COUNT;
  }

  // Compare against existing config, if we have one.
  if (config_) {
    // If versions are equal, we skip the update but it's not really
    // a failure.
    if (new_config->version_id() == config_->version_id())
      return UpdateResult::SKIPPED_VERSION_CHECK_EQUAL;

    // Check that version number increases
    if (new_config->version_id() <= config_->version_id()) {
      // TODO(crbug.com/347288618): Remove these, since they are not
      // expected to be useful long-term.
      base::UmaHistogramSparse(
          "SafeBrowsing.FileTypeUpdate.FailedUpdateSourceVersion",
          config_->version_id());
      base::UmaHistogramSparse(
          "SafeBrowsing.FileTypeUpdate.FailedUpdateDestinationVersion",
          new_config->version_id());
      return UpdateResult::FAILED_VERSION_CHECK;
    }

    // Check that we haven't dropped more than 1/2 the list.
    if (new_config->file_types().size() * 2 < config_->file_types().size())
      return UpdateResult::FAILED_DELTA_CHECK;
  }

  // Looks good. Update our internal list.
  SwapConfigLocked(new_config);

  return UpdateResult::SUCCESS;
}

void FileTypePolicies::SwapConfig(
    std::unique_ptr<DownloadFileTypeConfig>& new_config) {
  AutoLock lock(lock_);
  SwapConfigLocked(new_config);
}

void FileTypePolicies::SwapConfigLocked(
    std::unique_ptr<DownloadFileTypeConfig>& new_config) {
  lock_.AssertAcquired();
  config_.swap(new_config);

  // Build an index for faster lookup.
  file_type_by_ext_.clear();
  for (const DownloadFileType& file_type : config_->file_types()) {
    // If there are dups, first one wins.
    file_type_by_ext_.insert(std::make_pair(file_type.extension(), &file_type));
  }

  // |ext_map| is now stale since |file_type_by_ext_| has been cleared
  // and recreated, so clear it.
  policy::GetExtensionToPolicyMap().clear();
}

// static
base::FilePath::StringType FileTypePolicies::GetFileExtension(
    const base::FilePath& file) {
  // Remove trailing space and period characters from the extension.
  base::FilePath::StringType file_basename = file.BaseName().value();
  base::FilePath::StringPieceType trimmed_filename = base::TrimString(
      file_basename, FILE_PATH_LITERAL(". "), base::TRIM_TRAILING);
  return base::FilePath(trimmed_filename).FinalExtension();
}

// static
std::string FileTypePolicies::CanonicalizedExtension(
    const base::FilePath& file) {
  // The policy list is all ASCII, so a non-ASCII extension won't be in it.
  const base::FilePath::StringType ext = GetFileExtension(file);
  std::string ascii_ext =
      base::ToLowerASCII(base::FilePath(ext).MaybeAsASCII());
  if (ascii_ext[0] == '.')
    ascii_ext.erase(0, 1);
  return ascii_ext;
}

//
// Accessors
//

float FileTypePolicies::SampledPingProbability() const {
  AutoLock lock(lock_);
  return config_ ? config_->sampled_ping_probability() : 0.0;
}

const DownloadFileType& FileTypePolicies::PolicyForExtension(
    const std::string& ascii_ext,
    const GURL& source_url,
    const PrefService* prefs) const {
  lock_.AssertAcquired();
  // This could happen if the ResourceBundle is corrupted.
  if (!config_) {
    DCHECK(false);
    return last_resort_default_;
  }
  auto itr = file_type_by_ext_.find(ascii_ext);

  if (safe_browsing::IsInNotDangerousOverrideList(ascii_ext, source_url,
                                                  prefs)) {
    if (itr != file_type_by_ext_.end()) {
      return policy::GetOrCreatePolicyForExtensionOverrideNotDangerous(
          ascii_ext, *itr->second);
    }
    return policy::GetOrCreatePolicyForExtensionOverrideNotDangerous(
        ascii_ext, config_->default_file_type());
  }

  if (itr != file_type_by_ext_.end())
    return *itr->second;
  else
    return config_->default_file_type();
}

DownloadFileType FileTypePolicies::PolicyForFile(
    const base::FilePath& file,
    const GURL& source_url,
    const PrefService* prefs) const {
  const std::string ext = CanonicalizedExtension(file);
  AutoLock lock(lock_);
  return PolicyForExtension(ext, source_url, prefs);
}

DownloadFileType::PlatformSettings FileTypePolicies::SettingsForFile(
    const base::FilePath& file,
    const GURL& source_url,
    const PrefService* prefs) const {
  const std::string ext = CanonicalizedExtension(file);
  AutoLock lock(lock_);
  DCHECK_EQ(
      1, PolicyForExtension(ext, source_url, prefs).platform_settings().size());
  return PolicyForExtension(ext, source_url, prefs).platform_settings(0);
}

int64_t FileTypePolicies::UmaValueForFile(const base::FilePath& file) const {
  const std::string ext = CanonicalizedExtension(file);
  AutoLock lock(lock_);
  return PolicyForExtension(ext, GURL{}, nullptr).uma_value();
}

bool FileTypePolicies::IsArchiveFile(const base::FilePath& file) const {
  const std::string ext = CanonicalizedExtension(file);
  AutoLock lock(lock_);
  return PolicyForExtension(ext, GURL{}, nullptr).is_archive();
}

// TODO(nparker): Add unit tests for these accessors.

bool FileTypePolicies::IsAllowedToOpenAutomatically(
    const base::FilePath& file) const {
  const std::string ext = CanonicalizedExtension(file);
  if (ext.empty())
    return false;
  AutoLock lock(lock_);
  return PolicyForExtension(ext, GURL{}, nullptr)
             .platform_settings(0)
             .auto_open_hint() == DownloadFileType::ALLOW_AUTO_OPEN;
}

DownloadFileType::PingSetting FileTypePolicies::PingSettingForFile(
    const base::FilePath& file) const {
  const std::string ext = CanonicalizedExtension(file);
  AutoLock lock(lock_);
  return PolicyForExtension(ext, GURL{}, nullptr).ping_setting();
}

bool FileTypePolicies::IsCheckedBinaryFile(const base::FilePath& file) const {
  return PingSettingForFile(file) == DownloadFileType::FULL_PING;
}

DownloadFileType::DangerLevel FileTypePolicies::GetFileDangerLevel(
    const base::FilePath& file,
    const GURL& source_url,
    const PrefService* prefs) const {
  const std::string ext = CanonicalizedExtension(file);
  AutoLock lock(lock_);
  return PolicyForExtension(ext, source_url, prefs)
      .platform_settings(0)
      .danger_level();
}

uint64_t FileTypePolicies::GetMaxFileSizeToAnalyze(
    const std::string& ascii_ext) const {
  AutoLock lock(lock_);
  std::string inspection_ext;
  switch (PolicyForExtension(ascii_ext, GURL{}, nullptr).inspection_type()) {
    case DownloadFileType::NONE:
      break;
    case DownloadFileType::ZIP:
      inspection_ext = "zip";
      break;
    case DownloadFileType::RAR:
      inspection_ext = "rar";
      break;
    case DownloadFileType::DMG:
      inspection_ext = "dmg";
      break;
    case DownloadFileType::SEVEN_ZIP:
      inspection_ext = "7z";
      break;
  }

  return PolicyForExtension(inspection_ext, GURL{}, nullptr)
      .platform_settings(0)
      .max_file_size_to_analyze();
}

uint64_t FileTypePolicies::GetMaxFileSizeToAnalyze(
    const base::FilePath& file) const {
  return GetMaxFileSizeToAnalyze(CanonicalizedExtension(file));
}

uint64_t FileTypePolicies::GetMaxArchivedBinariesToReport() const {
  AutoLock lock(lock_);
  if (!config_ || !config_->has_max_archived_binaries_to_report()) {
    // The resource bundle may be corrupted.
    DCHECK(false);
    return 10;  // reasonable default
  }
  return config_->max_archived_binaries_to_report();
}

}  // namespace safe_browsing
