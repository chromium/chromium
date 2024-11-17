// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SPARKY_SPARKY_DELEGATE_H_
#define COMPONENTS_MANTA_SPARKY_SPARKY_DELEGATE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/manta/sparky/system_info_delegate.h"

namespace manta {

enum class PrefType {
  kNone = 0,
  kBoolean,
  kInt,
  kDouble,
  kString,
  kList,
  kDictionary,
  kMaxValue = kDictionary,
};

// Stores the setting data for the current or wanted state of a Pref.
struct COMPONENT_EXPORT(MANTA) SettingsData {
  SettingsData(const std::string& pref_name,
               const PrefType& pref_type,
               std::optional<base::Value> value);

  ~SettingsData();

  SettingsData(const SettingsData&);
  SettingsData& operator=(const SettingsData&);

  std::optional<base::Value> GetValue() const;

  std::string pref_name;
  PrefType pref_type;
  bool val_set{false};
  bool bool_val;
  int int_val;
  std::string string_val;
  double double_val;
};

struct COMPONENT_EXPORT(MANTA) FileData {
  FileData(const std::string& path,
           const std::string& name,
           const std::string& date_modified);

  ~FileData();

  FileData(const FileData&);
  FileData& operator=(const FileData&);

  std::string path;
  std::string name;
  std::optional<std::vector<uint8_t>> bytes;
  int64_t size_in_bytes;
  std::string date_modified;
  std::string summary;
};

using ScreenshotDataCallback =
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

struct COMPONENT_EXPORT(MANTA) AppsData {
  AppsData(const std::string& name, const std::string& id);

  ~AppsData();

  AppsData(AppsData&& other);
  AppsData& operator=(AppsData&& other);

  void AddSearchableText(const std::string& new_searchable_text);

  std::string name;
  std::string id;
  std::vector<std::string> searchable_text;
};
using StorageDataCallback =
    base::OnceCallback<void(std::unique_ptr<StorageData>)>;

using FilesDataCallback = base::OnceCallback<void(std::vector<FileData>)>;

// Virtual class to handle the information requests and actions taken within
// Sparky Provider which have a Chrome dependency.
class COMPONENT_EXPORT(MANTA) SparkyDelegate {
 public:
  using SettingsDataList = std::map<std::string, std::unique_ptr<SettingsData>>;
  SparkyDelegate();
  SparkyDelegate(const SparkyDelegate&) = delete;
  SparkyDelegate& operator=(const SparkyDelegate&) = delete;

  virtual ~SparkyDelegate();

  virtual bool SetSettings(std::unique_ptr<SettingsData> settings_data) = 0;
  virtual SettingsDataList* GetSettingsList() = 0;
  virtual std::optional<base::Value> GetSettingValue(
      const std::string& setting_id) = 0;
  virtual void GetScreenshot(ScreenshotDataCallback callback) = 0;
  virtual std::vector<AppsData> GetAppsList() = 0;
  virtual void LaunchApp(const std::string& app_id) = 0;
  virtual void ObtainStorageInfo(StorageDataCallback storage_callback) = 0;
  virtual void Click(int x, int y) = 0;
  virtual void KeyboardEntry(std::string text) = 0;
  virtual void KeyPress(const std::string& key,
                        bool control,
                        bool alt,
                        bool shift) = 0;
  virtual void GetMyFiles(FilesDataCallback callback,
                          bool obtain_bytes,
                          std::set<std::string> allowed_file_paths) = 0;
  virtual void LaunchFile(const std::string& file_path) = 0;
  virtual void WriteFile(const std::string& name, const std::string& bytes) = 0;
  virtual void UpdateFileSummaries(
      const std::vector<manta::FileData>& files_with_summary) = 0;
  virtual std::vector<manta::FileData> GetFileSummaries() = 0;
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SPARKY_SPARKY_DELEGATE_H_
