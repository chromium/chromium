// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"

class GURL;
class PrefService;

namespace safe_browsing {

struct FileTypePoliciesSingletonTrait;
class FileTypePoliciesTestOverlay;

// This holds a list of file types (aka file extensions) that we know about,
// with policies related to how Safe Browsing and the download UI should treat
// them.
//
// The data to populate it is read from a ResourceBundle and then also
// fetched periodically from Google to get the most up-to-date policies.
//
// This is thread safe. We assume it is updated at most every few hours.

class FileTypePolicies {
 public:
  FileTypePolicies(const FileTypePolicies&) = delete;
  FileTypePolicies& operator=(const FileTypePolicies&) = delete;

  virtual ~FileTypePolicies();

  static FileTypePolicies* GetInstance();  // Singleton

  // Update the internal list from a binary proto fetched from the network.
  // Same integrity checks apply. This can be called multiple times with new
  // protos.
  void PopulateFromDynamicUpdate(const std::string& binary_pb);

  //
  // Static Utils
  //

  // Returns the final extension with the leading dot, after stripping
  // trailing dots and spaces.  It is difference from FilePath::Extension()
  // and FilePath::FinalExtension().
  // TODO(nparker): Consolidate. Maybe add this code to FinalExtension().
  static base::FilePath::StringType GetFileExtension(
      const base::FilePath& file);

  //
  // Accessors
  //
  bool IsArchiveFile(const base::FilePath& file) const;

  // SBClientDownloadExtensions UMA histogram bucket for this file's type.
  int64_t UmaValueForFile(const base::FilePath& file) const;

  // True if download protection should send a ping to check
  // this type of file.
  bool IsCheckedBinaryFile(const base::FilePath& file) const;

  // True if the user can select this file type to be opened automatically.
  bool IsAllowedToOpenAutomatically(const base::FilePath& file) const;

  // Return the danger level of this file type.
  DownloadFileType::DangerLevel GetFileDangerLevel(
      const base::FilePath& file,
      const GURL& source_url,
      const PrefService* prefs) const;

  // Return the type of ping we should send for this file
  DownloadFileType::PingSetting PingSettingForFile(
      const base::FilePath& file) const;

  float SampledPingProbability() const;

  DownloadFileType PolicyForFile(const base::FilePath& file,
                                 const GURL& source_url,
                                 const PrefService* prefs) const;
  DownloadFileType::PlatformSettings SettingsForFile(
      const base::FilePath& file,
      const GURL& source_url,
      const PrefService* prefs) const;

  // Return max size for which unpacking and/or binary feature extraction is
  // supported for the given file extension.
  uint64_t GetMaxFileSizeToAnalyze(const std::string& ascii_ext) const;
  uint64_t GetMaxFileSizeToAnalyze(const base::FilePath& path) const;

  // Return max number of archived_binaries we should add to a download ping.
  uint64_t GetMaxArchivedBinariesToReport() const;

 protected:
  // Creator must call one of Populate* before calling other methods.
  FileTypePolicies();

  // Used in metrics, do not reorder.
  enum class UpdateResult {
    SUCCESS = 1,
    FAILED_EMPTY = 2,
    FAILED_PROTO_PARSE = 3,
    FAILED_DELTA_CHECK = 4,
    FAILED_VERSION_CHECK = 5,
    FAILED_DEFAULT_SETTING_SET = 6,
    FAILED_WRONG_SETTINGS_COUNT = 7,
    SKIPPED_VERSION_CHECK_EQUAL = 8,
  };

  // Read data from an serialized protobuf and update the internal list
  // only if it passes integrity checks.
  virtual UpdateResult PopulateFromBinaryPb(const std::string& binary_pb);

  // Fetch the blob from the main resource bundle.
  virtual std::string ReadResourceBundle();

  // Record the result of an update attempt.
  virtual void RecordUpdateMetrics(UpdateResult result,
                                   const std::string& src_name);

  // Return the ASCII lowercase extension w/o leading dot, or empty.
  static std::string CanonicalizedExtension(const base::FilePath& file);

  // Look up the policy for a given ASCII ext.
  virtual const DownloadFileType& PolicyForExtension(
      const std::string& ext,
      const GURL& source_url,
      const PrefService* prefs) const;

 private:
  // Swap in a different config. This will rebuild file_type_by_ext_ index.
  void SwapConfig(std::unique_ptr<DownloadFileTypeConfig>& new_config);
  void SwapConfigLocked(std::unique_ptr<DownloadFileTypeConfig>& new_config);

  // Read data from the main ResourceBundle. This updates the internal list
  // only if the data passes integrity checks. This is normally called once
  // after construction.
  void PopulateFromResourceBundle();

  // The latest config we've committed. Starts out null.
  // Protected by lock_.
  std::unique_ptr<DownloadFileTypeConfig> config_;

  // This references entries in config_.
  // Protected by lock_.
  std::map<std::string, raw_ptr<const DownloadFileType, CtnExperimental>>
      file_type_by_ext_;

  // Type used if we can't load from disk.
  // Written only in the constructor.
  DownloadFileType last_resort_default_;

  mutable base::Lock lock_;

  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest, UnpackResourceBundle);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest, BadProto);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest, BadUpdateFromExisting);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest,
                           NoInspectionTypeReturnsDefault);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest,
                           ChecksInspectionTypeNotDefault);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest,
                           NotDangerousOverrideShouldOnlyOverrideDangerType);

  friend struct FileTypePoliciesSingletonTrait;
  friend class FileTypePoliciesTestOverlay;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_H_
