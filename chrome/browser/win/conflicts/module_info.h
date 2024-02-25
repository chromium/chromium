// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_INFO_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_INFO_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "content/public/common/process_type.h"

// ModuleInfoKey and ModuleInfoData are used in pair by the ModuleDatabase to
// maintain information about a module, usually in a std::map.

// This is the constant portion of the module information, and is used to
// uniquely identify one.
struct ModuleInfoKey {
  ModuleInfoKey(const base::FilePath& module_path,
                uint32_t module_size,
                uint32_t module_time_date_stamp);

  // Less-than operator allowing this object to be used in std::map.
  bool operator<(const ModuleInfoKey& mi) const;
  // Allows comparing keys in DCHECKs.
  bool operator==(const ModuleInfoKey& mi) const;

  // Full path to the module on disk. Part of the key for a ModuleInfo.
  base::FilePath module_path;

  // The module size. Part of the key for a ModuleInfo. This is taken from
  // SizeOfImage from the module's IMAGE_OPTIONAL_HEADER.
  uint32_t module_size;

  // The module time date stamp. Part of the key for a ModuleInfo. Taken from
  // TimeDateStamp from the module's IMAGE_FILE_HEADER.
  uint32_t module_time_date_stamp;
};

// Holds more detailed information about a given module. Because all of this
// information is expensive to gather and requires disk access, it should be
// collected via InspectModule() on a task runner that allow blocking.
//
// Note: Any modification to this structure should be reflected in
//       SerializeInspectionResult() and DeserializeInspectionResult() in
//       chrome/browser/win/conflicts/inspection_results_cache.cc.
struct ModuleInspectionResult {
  ModuleInspectionResult();
  ModuleInspectionResult(const ModuleInspectionResult& other);
  ModuleInspectionResult(ModuleInspectionResult&& other);

  ModuleInspectionResult& operator=(const ModuleInspectionResult& other);
  ModuleInspectionResult& operator=(ModuleInspectionResult&& other);

  ~ModuleInspectionResult();

  // The lowercase module path, not including the basename.
  std::u16string location;

  // The basename of the module.
  std::u16string basename;

  // The name of the product the module belongs to.
  std::u16string product_name;

  // The module file description.
  std::u16string description;

  // The module version. This is usually in the form a.b.c.d (where a, b, c and
  // d are integers), but may also have fewer than 4 components.
  std::u16string version;

  // The certificate info for the module.
  CertificateInfo certificate_info;
};

// Contains the inspection result of a module and additional information that is
// useful to the ModuleDatabase.
struct ModuleInfoData {
  // Different properties that the module can have. Used as bit set values.
  enum ModuleProperty : uint32_t {
    // These modules are or were loaded into one of chrome's process at some
    // point.
    kPropertyLoadedModule = 1 << 0,
    // These modules are registered as a shell extension.
    kPropertyShellExtension = 1 << 1,
    // These modules are registered as an Input Method Editor.
    kPropertyIme = 1 << 2,
    // The module was added to the module blocklist cache.
    kPropertyAddedToBlocklist = 1 << 3,
    // These modules were blocked from loading into the process.
    kPropertyBlocked = 1 << 4,
  };

  ModuleInfoData();
  ~ModuleInfoData();

  ModuleInfoData(ModuleInfoData&& module_data) noexcept;

  // Set of all process types in which this module has been seen (may not be
  // currently present in a process of that type). This is a conversion of
  // ProcessType enumeration to a bitfield. See "ProcessTypeToBit" and
  // "BitIndexToProcessType" for details.
  uint32_t process_types;

  // Set that describes the properties of the module.
  uint32_t module_properties;

  // The inspection result obtained via InspectModule().
  std::optional<ModuleInspectionResult> inspection_result;
};

// Given a module located at |module_path|, returns a populated
// ModuleInspectionResult that contains detailed information about the module on
// disk. This is a blocking task that requires access to disk.
ModuleInspectionResult InspectModule(const base::FilePath& module_path);

// Returns the date stamp to be used in the inspection results cache.
// Represents the number of hours between |time| and the Windows epoch
// (1601-01-01 00:00:00 UTC).
uint32_t CalculateTimeStamp(base::Time time);

// Generate the code id of a module.
std::string GenerateCodeId(const ModuleInfoKey& module_key);

// Converts a valid |process_type| to a bit for use in a bitmask of process
// values. Exposed in the header for testing.
uint32_t ProcessTypeToBit(content::ProcessType process_type);

// Converts a |bit_index| (which maps to the bit 1 << bit_index) to the
// corresponding process type. Exposed in the header for testing.
content::ProcessType BitIndexToProcessType(uint32_t bit_index);

// Returns true if |process_types| has at least one bit corresponding to a
// process type where the blocking of third-party modules is enabled.
bool IsBlockingEnabledInProcessTypes(uint32_t process_types);

namespace internal {

// Normalizes the information already contained in |inspection_result|. In
// particular:
// - The path is split in 2 parts: The basename and the location.
// - If it uses commas, the version string is modified to use periods.
// - If there is one, the version string suffix is removed.
//
// Exposed for testing.
void NormalizeInspectionResult(ModuleInspectionResult* inspection_result);

}  // namespace internal

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_INFO_H_
