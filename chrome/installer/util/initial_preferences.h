// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains functions processing initial preference file used by
// setup and first run.

#ifndef CHROME_INSTALLER_UTIL_INITIAL_PREFERENCES_H_
#define CHROME_INSTALLER_UTIL_INITIAL_PREFERENCES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {
class FilePath;
}  // namespace base

namespace installer {

// The initial preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function parses the distribution
// section of the preferences file.
//
// A prototypical initial preferences file looks like this:
//
// {
//   "distribution": {
//      "create_all_shortcuts": true,
//      "do_not_launch_chrome": false,
//      "import_bookmarks_from_file": "c:\\path",
//      "make_chrome_default_for_user": true,
//      "ping_delay": 40,
//      "require_eula": true,
//      "skip_first_run_ui": true,
//      "system_level": false,
//      "verbose_logging": true
//   },
//   "browser": {
//      "show_home_button": true
//   },
//   "bookmark_bar": {
//      "show_on_all_tabs": true
//   },
//   "first_run_tabs": [
//      "http://gmail.com",
//      "https://igoogle.com"
//   ],
//   "homepage": "http://example.org",
//   "homepage_is_newtabpage": false,
//   "import_autofill_form_data": false,
//   "import_bookmarks": false,
//   "import_history": false,
//   "import_home_page": false,
//   "import_saved_passwords": false,
//   "import_search_engine": false
// }
//
// A reserved "distribution" entry in the file is used to group related
// installation properties. This entry will be ignored at other times.

class InitialPreferences {
 public:
#if !BUILDFLAG(IS_MAC)
  // Find and return initial preferences's file path that is located in `dir`.
  // It will fallback to the legacy file name if the new one is not available
  // and `for_read` is set to true.
  // Only available on Windows, Linux and CrOS. Mac has its own initial
  // preferences file names which is implemented in
  // `chrome/browser/mac/initial_prefs.h`.
  static base::FilePath Path(const base::FilePath& dir, bool for_read = true);
#endif  // !BUILDFLAG(IS_MAC)

  // Construct a initial preferences from the current process' current command
  // line. Equivalent to calling
  // InitialPreferences(*CommandLine::ForCurrentProcess()).
  InitialPreferences();

  // Parses the command line and optionally reads the initial preferences file
  // to get distribution related install options (if the "installerdata" switch
  // is present in the command line.
  // The options from the preference file and command line are merged, with the
  // ones from the command line taking precedence in case of a conflict.
  explicit InitialPreferences(const base::CommandLine& cmd_line);

  // Parses a specific preferences file and does not merge any command line
  // switches with the distribution dictionary.
  explicit InitialPreferences(const base::FilePath& prefs_path);

  // Parses a preferences directly from |prefs| and does not merge any command
  // line switches with the distribution dictionary.
  explicit InitialPreferences(const std::string& prefs);

  // Parses a preferences directly from |prefs| and does not merge any command
  // line switches with the distribution dictionary.
  explicit InitialPreferences(base::Value::Dict prefs);

  InitialPreferences(const InitialPreferences&) = delete;
  InitialPreferences& operator=(const InitialPreferences&) = delete;

  ~InitialPreferences();

  // Each of the Get methods below returns true if the named value was found in
  // the distribution dictionary and its value assigned to the 'value'
  // parameter.  If the value wasn't found, the return value is false.
  bool GetBool(const std::string& name, bool* value) const;
  bool GetInt(const std::string& name, int* value) const;
  bool GetString(const std::string& name, std::string* value) const;
  bool GetPath(const std::string& name, base::FilePath* value) const;

  // As part of the initial preferences an optional section indicates the tabs
  // to open during first run. An example is the following:
  //
  //  {
  //    "first_run_tabs": [
  //       "http://google.com/f1",
  //       "https://google.com/f2"
  //    ]
  //  }
  //
  // Note that the entries are usually urls but they don't have to be.
  //
  // An empty vector is returned if the first_run_tabs preference is absent.
  std::vector<std::string> GetFirstRunTabs() const;

  // The initial preferences can also contain a list of extension ids block. If
  // so, the extensions listed there will be installed during the first run
  // experience.
  //
  // An example is the following:
  //
  // {
  //   "initial_extensions": {
  //     "provider_name": "ABCXYZ Provider",
  //     "list": [
  //        "ppflmjolhbonpkbkooiamcnenbmbjcbb"
  //      ]
  //   }
  // }
  std::string GetInitialExtensionsProviderName() const;
  const base::Value::List* GetInitialExtensionsList() const;

  // The initial preferences file can include a bookmarks block that gets
  // imported on the first run. This block contains bookmark and folder nodes
  // that get recursively visited and imported.
  //
  // Bookmark nodes must have 'name', 'type' (set to 'url') and 'url' fields.
  // Nodes can optionally contain 'icon_data_url' with base64 encoded favicon
  // data URL that is parsed and shown until the user visits the bookmark's url
  // and loads a live, fresh version.
  // If not including 'icon_data_url', or it is not in correctly encoded,
  // default favicon is shown.
  //
  // Folder nodes must have 'name', 'type' (set to 'folder') and 'children'
  // fields.
  //
  // Example block:
  // {
  //   "bookmarks": {
  //     "first_run_bookmarks": {
  //       "children": [
  //         {
  //           "name": "ABC",
  //           "type": "url",
  //           "url": "https://google.com",
  //           "icon_data_url": "data:image/png;base64,iVBORw0KGgoAAAANSUhEU..."
  //         },
  //         {
  //           "name": "XYZ",
  //           "type": "url",
  //           "url": "https://facebook.com"
  //         },
  //         {
  //           "name": "Folder 1",
  //           "type": "folder",
  //           "children": [
  //             {
  //               "name": "ABC",
  //               "type": "url",
  //               "url": "https://google.com"
  //             },
  //             {
  //               "name": "Folder 2",
  //               "type": "folder",
  //               "children": [
  //                 {
  //                   "name": "ABC",
  //                   "type": "url",
  //                   "url": "https://google.com"
  //                 }
  //               ]
  //             }
  //           ]
  //         }
  //       ]
  //     }
  //   }
  // }
  //
  // The return value can be a nullptr if this dict is not specified in the
  // initial preferences file.
  const base::Value::Dict* GetBookmarksBlock() const;

  // Returns the compressed variations seed entry from the initial prefs.
  std::string GetCompressedVariationsSeed();

  // Returns the variations seed signature entry from the initial prefs.
  std::string GetVariationsSeedSignature();

  // Returns true iff the initial preferences were successfully read from a
  // file.
  bool read_from_file() const { return preferences_read_from_file_; }

  // Returns a reference to this InitialPreferences' root dictionary of values.
  const base::Value::Dict& initial_dictionary() const {
    return *initial_dictionary_;
  }

  // Returns a static preference object that has been initialized with the
  // CommandLine object for the current process.
  // NOTE: Must not be called before CommandLine::Init() is called!
  // OTHER NOTE: Not thread safe.
  static const InitialPreferences& ForCurrentProcess();

 private:
  void InitializeFromCommandLine(const base::CommandLine& cmd_line);
  void InitializeFromFilePath(const base::FilePath& prefs_path);

  // Initializes the instance from a given JSON string, returning true if the
  // string was successfully parsed.
  bool InitializeFromString(const std::string& json_data);

  // Enforces legacy preferences that should no longer be used, but could be
  // found in older initial preferences files.
  void EnforceLegacyPreferences();

  // Removes the specified string pref from the initial preferences and returns
  // its value. Should be used for initial prefs that shouldn't be automatically
  // copied over to profile preferences.
  std::string ExtractPrefString(const std::string& name);

  std::optional<base::Value::Dict> initial_dictionary_;
  raw_ptr<base::Value::Dict> distribution_ = nullptr;
  bool preferences_read_from_file_ = false;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INITIAL_PREFERENCES_H_
