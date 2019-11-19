// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PUP_DATA_PUP_DATA_H_
#define CHROME_CHROME_CLEANER_PUP_DATA_PUP_DATA_H_

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/os/disk_util_types.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace chrome_cleaner {

class TestPUPData;
class SignatureMatcherAPI;
class UwSCatalog;

// This class exposes the data necessary to identify and remove Potentially
// Unwanted Programs (PUPs). We use a class instead of the null terminating
// C-Arrays directly so that we can filter based on UwSId's when only a subset
// of the PUPs are needed (e.g., the cleaner module is only interested in the
// PUPs that scanner has found).
class PUPData {
 public:
  // These flags are applied to whole PUPs.
  typedef uint32_t Flags;
  enum : Flags {
    FLAGS_NONE = 0,
    // Used when a PUP should be removed, otherwise it is only reported.
    FLAGS_ACTION_REMOVE = 1 << 0,
    // Used when a PUP should always be removed post-reboot, even when the
    // Restart Manager doesn't think a reboot is needed.
    FLAGS_REMOVAL_FORCE_REBOOT = 1 << 1,
    // When the UwS is detected activate the detailed system report.
    DEPRECATED_FLAGS_DETAILED_REPORT = 1 << 2,
    // Used for PUPs that interfere with the prompt, so we don't want to report
    // that a cleanup can be done when they are present. This only affects the
    // reporter, not the removal tool.
    DEPRECATED_FLAGS_INHIBIT_REPORTING = 1 << 3,
    // Used for PUPs that interfere with the cleaner, so we don't want to offer
    // a removal to the user when they are present. This affects both the
    // reporter and the removal tool.
    DEPRECATED_FLAGS_INHIBIT_REMOVAL = 1 << 4,
    // Behaviour that breaks our UwS policies
    // (https://www.google.com/about/unwanted-software-policy.html) has been
    // observed and confirmed. PUP's without this flag are still being
    // investigated and must not be removed.
    FLAGS_STATE_CONFIRMED_UWS = 1 << 5,
    // The UwS is considered a malware.
    DEPRECATED_FLAGS_CATEGORY_MALWARE = 1 << 6,
    // The UwS is hijacking the browser.
    DEPRECATED_FLAGS_CATEGORY_BROWSER_HIJACKER = 1 << 7,
    // The UwS is injecting an extension.
    DEPRECATED_FLAGS_CATEGORY_EXTENSIONS_INJECTOR = 1 << 8,
    // The UwS is injecting ads in webpages.
    DEPRECATED_FLAGS_CATEGORY_ADS_INJECTOR = 1 << 9,
    // The UwS is a scareware.
    DEPRECATED_FLAGS_CATEGORY_SCAREWARE = 1 << 10,
    // When adding new flags here, please update
    // chrome_cleaner/proto/uws_spec_by_version.proto.
  };

  // A matching rule describes how the content of a disk footprint is matched.
  enum DiskMatchRule {
    // An invalid matching rule.
    DISK_MATCH_INVALID = 0,
    // Any file will match. So a folder is always marked as found, as long as it
    // contains at least one file.
    DISK_MATCH_ANY_FILE,
    // Only binary files will match. So a folder is only marked as found if it
    // contains at least one binary file (i.e., .exe, .dll, or .sys).
    // Note, this can't be used as part of the scanning footprints for a removal
    // signature (it can be used with a remove only footprint).
    DISK_MATCH_BINARY_FILE,
    // When the file path is found, clean containing folder X levels up. E.g.,
    // if the path is C:\a\b\c\d.exe, then for DISK_MATCH_FILE_IN_FOLDER_DEPTH_1
    // the whole folder c:\a\b\c is deleted, but for
    // DISK_MATCH_FILE_IN_FOLDER_DEPTH_3 c:\a is deleted. Only paths to files
    // are
    // supported now, this won't match a folder within a folder.
    DISK_MATCH_FILE_IN_FOLDER_DEPTH_1,
    DISK_MATCH_FILE_IN_FOLDER_DEPTH_2,
    DISK_MATCH_FILE_IN_FOLDER_DEPTH_3,
    DISK_MATCH_FILE_IN_FOLDER_DEPTH_4,
    // DISK_MATCH_FILE_IN_FOLDER_END should only be used to indicate the end
    // value for file in folder depths.
    DISK_MATCH_FILE_IN_FOLDER_END,
  };

  // This invalid UwSId is used to identify the end of a PUP array.
  static const UwSId kInvalidUwSId = 0xFFFFFFFF;

  // This id indicates when a footprint isn't associated with any flavour.
  static const int kNoFlavour = -1;

  // The default number of active files to be willing to remove for various
  // UwS installation sizes.
  static const size_t kMaxFilesToRemoveLargeUwS = 200;
  static const size_t kMaxFilesToRemoveMediumUwS = 100;
  static const size_t kMaxFilesToRemoveSmallUwS = 50;

  // This id is used when a StaticDiskFootprint doesn't have a CSIDL.
  static const int kInvalidCsidl = -1;

  // The delimiters used to split registry values.
  static const wchar_t kCommaDelimiter[];
  static const size_t kCommaDelimiterLength;
  static const wchar_t kCommonDelimiters[];
  static const size_t kCommonDelimitersLength;

  // The character used to escape wild-cards into registry key name patterns.
  static const wchar_t kRegistryPatternEscapeCharacter;

  // A disk footprint is just a path.
  struct StaticDiskFootprint {
    // A flavour is used to indicate what footprints should be grouped and
    // removed together.
    int flavour;
    // A CSIDL value identifying the root of |path| if it is a relative path.
    // |csidl| is simply ignored when |path| is an absolute path or is null.
    int csidl;
    // Is only null for the last entry identifying the end of the array.
    // It can be either an absolute path, or a relative path, in which case,
    // |csidl| is used to get the proper root.
    const wchar_t* path;
    // Describe the way |path| is used to match the disk footprint.
    DiskMatchRule rule;
  };

  // Information stored in the registry. If |value_name| is empty, the whole
  // key at |key_path| is the footprint. If |value_substring| is not empty,
  // then |value_name| can not be empty, and only the substring in
  // |value_substring| is scanned for, or removed. For example, we may want to
  // only remove the PUP path from the AppInit_DLLs registry entry, and leave
  // other paths in there. When |value_substring| is null but not |value_name|,
  // then the whole value is to be removed.
  struct StaticRegistryFootprint {
    // A flavour is used to indicate what footprints should be grouped and
    // removed together.
    int flavour;
    // The root to use for this registry footprint. Ignored if |key_path| is
    // null.
    RegistryRoot registry_root;
    // Is only null for the last entry identifying the end of the array.
    const wchar_t* key_path;
    // Can be null if the whole key is to be removed. Can not be null when
    // |value_substring| is not null.
    const wchar_t* value_name;
    // Can be null if the whole value is to be removed.
    const wchar_t* value_substring;
    // Describe the way |value_substring| is used to match the content of the
    // registry value.
    RegistryMatchRule rule;
  };

  // Represents a path to be matched on disk using regular expressions. There is
  // a base path, which is either |csidl|/|path| if |path| is relative, or just
  // |path| if it is absolute. Then, the |regex_components| will be used to
  // match the remainder of the full path. |regex_components| can use
  // backreferences into previous components, as they will be successively
  // merged into the regex used to match the final path.
  struct MatchPath {
    // A CSIDL value identifying the root of |path| if it is a relative path.
    // |csidl| is simply ignored when |path| is an absolute path.
    int csidl;
    // Is only null for the last entry identifying the end of the array.
    // Can be either an absolute path, or a relative path, in which case,
    // |csidl| is used to get the proper root. Regex matching will start at this
    // path (but |path| itself is not a regex).
    const wchar_t* path;
    // The path components to be matched. A null entry indicates the end of the
    // list. These components must not use path separators. As the filesystem
    // gets enumerated, the components will be merged into one regex, which
    // means that backreferences of groups matched in previous components will
    // work.
    const wchar_t** regex_components;
  };

  // This is the expanded version of |StaticRegistryFootprint|. |key_path| and
  // |value_name| can no longer contain wildcards.
  struct RegistryFootprint {
    RegistryFootprint();
    RegistryFootprint(const RegKeyPath& key_path,
                      const base::string16& value_name,
                      const base::string16& value_substring,
                      RegistryMatchRule rule);
    RegistryFootprint(const RegistryFootprint& other);
    ~RegistryFootprint();

    RegistryFootprint& operator=(const RegistryFootprint& other);

    bool operator==(const RegistryFootprint& other) const;

    // Must not be empty.
    RegKeyPath key_path;
    // Can be empty for default value or when value is unused.
    base::string16 value_name;
    // Can be empty when unused.
    base::string16 value_substring;
    // Describe the way |value_substring| is used to match the content of the
    // registry value.
    RegistryMatchRule rule;
  };

  // Forward declaration.
  class PUP;

  // A container for PUP data, allows fast access.
  typedef std::unordered_map<UwSId, std::unique_ptr<PUP>> PUPDataMap;

  // The type of the function to execute the custom matcher. The |pup| structure
  // should be filled with the found footprint. The |active_footprint_found|
  // must be set to true when non-leftover footprint are found.
  // Return false on failure. On failure, the whole scan and remove process is
  // interrupted for this PUP.
  typedef bool (*CustomMatcher)(const MatchingOptions& options,
                                const SignatureMatcherAPI* signature_matcher,
                                PUP* pup,
                                bool* active_footprint_found);

  // All the information and footprints of a single UwS.
  //
  // The fields of this structure should point to static memory so that they
  // remain valid as UwSSignature objects are copied. This structure cannot
  // have an explicit constructor because it is commonly aggregate-initialized.
  struct UwSSignature {
    UwSId id = 0;
    Flags flags = 0;
    // The name of the UwS. Can be null for anonymous UwS.
    const char* name = nullptr;
    // An upper limit on the number of files allowed to be deleted for the UwS
    // to avoid damage caused by internal errors or malware interactions.
    size_t max_files_to_remove = 0;
    // The following arrays are terminated by an entry with a null [key_]path.
    const StaticDiskFootprint* disk_footprints = nullptr;
    const StaticRegistryFootprint* registry_footprints = nullptr;
    // Custom matchers to scan UwS with dynamic footprints.
    const CustomMatcher* custom_matchers = nullptr;
  };

  struct FileInfo {
    FileInfo();
    FileInfo(const FileInfo&);
    explicit FileInfo(const std::set<UwS::TraceLocation>& found_in);
    ~FileInfo();

    bool operator==(const FileInfo& other) const;

    std::set<UwS::TraceLocation> found_in;
  };

  // The data that was matched for an UwS during scanning.
  //
  // PUP objects are created in the engine sandbox process as the engine finds
  // UwS, and then copied to the broker process using the Mojo interface in
  // interfaces/pup.mojom.
  //
  // For the legacy scanning engine PUP objects are created directly in the
  // sandbox broker process.
  class PUP {
   public:
    typedef FilePathMap<FileInfo> FileInfoMap;

    // Default constructor required by Mojo. Initializes signature with a null
    // pointer, since no information encoded there needs to be transmitted.
    PUP();
    explicit PUP(const UwSSignature* signature);
    PUP(const PUP& other);
    ~PUP();

    PUP& operator=(const PUP& other);

    // Static data for an UwS. This will be empty in the sandbox target
    // process. In the broker process the UwSSignature is looked up by UwSId
    // and added to the PUP structure.
    const UwSSignature& signature() const { return *signature_; }

    // Add the given |file_path| to |expanded_disk_footprints|. Return false if
    // |file_path| was already in the set.
    bool AddDiskFootprint(const base::FilePath& file_path);
    void ClearDiskFootprints();

    // Mark disk footprint |file_path| as detected in |location|.
    void AddDiskFootprintTraceLocation(const base::FilePath& file_path,
                                       UwS::TraceLocation location);

    // Add all matching data from |other| to the current pup.
    void MergeFrom(const PUP& other);

    // The set of expanded disk footprints generated by the scanner. Populated
    // in the target process when the engine is sandboxed.
    FilePathSet expanded_disk_footprints;

    // The set of expanded registry footprints generated by the scanner. Only
    // populated by the legacy unsandboxed engine.
    std::vector<RegistryFootprint> expanded_registry_footprints;

    // The list of expanded scheduled task names generated by the scanner.
    // Only populated by the legacy unsandboxed engine.
    std::vector<base::string16> expanded_scheduled_tasks;

    // Mapping from detected files to where they were found. Populated in the
    // target process when the engine is sandboxed.
    FileInfoMap disk_footprints_info;

    // List of UwE found by the scanner. Populated in the broker process after
    // the PUPData is copied from the target process.
    std::vector<ForceInstalledExtension> matched_extensions;

   protected:
    // Allow PUPData to update |signature_| when UpdateCachedUwSForTesting is
    // called. The signature pointers can be invalidated when TestPUPData
    // creates new test UwS, causing an existing vector of test UwS to be
    // resized.
    friend PUPData;

    const UwSSignature* signature_;
  };

  PUPData();
  ~PUPData();

  using UwSCatalogs = std::vector<const UwSCatalog*>;

  // Loads cached information from the given |uws_catalogs|. Must be called in
  // order to use static functions of PUPData. Calling it again will delete all
  // cached PUP structures, which will lose their state (detected disk
  // footprints, etc.)
  static void InitializePUPData(const UwSCatalogs& uws_catalogs);

  // Returns the |uws_catalogs| used in the last call to InitializePUPData.
  static const UwSCatalogs& GetUwSCatalogs() {
    DCHECK(last_uws_catalogs_);
    return *last_uws_catalogs_;
  }

  // Returns true if |uws_id| corresponds to a cached pup.
  static bool IsKnownPUP(UwSId uws_id);

  // Retrieves information about a given PUP. The ID must be valid otherwise
  // the function generates an error. The data returned is still owned by
  // PUPData.
  static PUP* GetPUP(UwSId uws_id);

  // Returns a vector of UwSId's used to iterate over all pups.
  static const std::vector<UwSId>* GetUwSIds();

  // Return the name of |pup| if it has one, and "???" otherwise.
  static const char* GetPUPName(const PUP* pup);

  // Return whether |flags| identify a report only PUPs.
  static bool HasReportOnlyFlag(Flags flags);

  // Return whether |flags| identify PUPs to remove.
  static bool HasRemovalFlag(Flags flags);

  // Return whether |flags| identify PUPs that needs a reboot.
  static bool HasRebootFlag(Flags flags);

  // Return whether |flags| identify PUPs that has been confirmed malicious.
  static bool HasConfirmedUwSFlag(Flags flags);

  // Return whether |uws_id| corresponds to a report only UwS.
  static bool IsReportOnlyUwS(UwSId uws_id);

  // Return whether |uws_id| corresponds to a removable UwS.
  static bool IsRemovable(UwSId uws_id);

  // Return whether |uws_id| corresponds to a confirmed malicious UwS.
  static bool IsConfirmedUwS(UwSId uws_id);

  // Copy PUP ids to |output| when the predicate |chooser| returns true.
  static void ChoosePUPs(const std::vector<UwSId>& input_pup_list,
                         bool (*chooser)(Flags),
                         std::vector<UwSId>* output);

  // Return whether there is at least one PUP with flags matched by |chooser|.
  static bool HasFlaggedPUP(const std::vector<UwSId>& input_pup_list,
                            bool (*chooser)(Flags));

  // Convert a RegistryRoot to its corresponding HKEY. If
  // |registry_root| is a group policy, and |policy_file| is not null, the path
  // to the group policy file is set in |policy_file|.
  // Return false on failure.
  static bool GetRootKeyFromRegistryRoot(RegistryRoot registry_root,
                                         HKEY* key,
                                         base::FilePath* policy_file);

  // Add a dynamic registry footprint to delete the registry key |key_path|.
  static void DeleteRegistryKey(const RegKeyPath& key_path, PUP* pup);

  // Add a dynamic registry footprint to delete the registry key |key_path|,
  // only if the key exists.
  static void DeleteRegistryKeyIfPresent(const RegKeyPath& key_path, PUP* pup);

  // Add a dynamic registry footprint to delete the registry value
  // |key_path|/|value_name|.
  static void DeleteRegistryValue(const RegKeyPath& key_path,
                                  const wchar_t* value_name,
                                  PUP* pup);

  // Add a dynamic registry footprint to delete the registry value
  // |key_path|/|value_name| with |rule|.
  static void UpdateRegistryValue(const RegKeyPath& key_path,
                                  const wchar_t* value_name,
                                  const wchar_t* value_substring,
                                  RegistryMatchRule rule,
                                  PUP* pup);

  // Add a dynamic footprint to delete a scheduled task.
  static void DeleteScheduledTask(const wchar_t* task_name, PUP* pup);

  // Return the engine this UwSId belongs to.
  static Engine::Name GetEngine(UwSId id);

 private:
  // So that tests can call UpdateCachedUwSForTesting to add new test UwS.
  friend TestPUPData;

  // Returns a cached map of PUPs from static arrays and caches it in a map.
  // Exposed for testing.
  static const PUPData::PUPDataMap* GetAllPUPs();

  // Updates all cached UwSSignature objects to point to the current contents
  // of the catalogs. Creates new PUP objects and adds them to the cache for
  // any UwSSignature's that are not already in the cache. Does not delete any
  // cached UwS; for that, call InitializePUPData again.
  //
  // This can be used when new test UwS is created to add that UwS to the cache
  // without invalidating any existing PUP objects that may already contain
  // expanded disk footprints.
  static void UpdateCachedUwSForTesting();

  static void AddPUPToMap(std::unique_ptr<PUPData::PUP> pup);

  // Cached PUPData information.
  static PUPDataMap* cached_pup_map_;

  // Cached UwSId list.
  static std::vector<UwSId>* cached_uws_ids_;

  static UwSCatalogs* last_uws_catalogs_;

  DISALLOW_COPY_AND_ASSIGN(PUPData);
};

// This macro makes it easier to create strings with the
// kRegistryPatternEscapeCharacter.
#define ESCAPE_REGISTRY_STR(str) L"\uFFFF" L##str

const PUPData::StaticDiskFootprint kNoDisk[] = {{}};
const PUPData::StaticRegistryFootprint kNoRegistry[] = {{}};
const PUPData::CustomMatcher kNoCustomMatcher[] = {{}};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PUP_DATA_PUP_DATA_H_
