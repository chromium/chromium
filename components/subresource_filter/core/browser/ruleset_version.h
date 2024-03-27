// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_VERSION_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_VERSION_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

class PrefRegistrySimple;
class PrefService;

namespace base::trace_event {
class TracedValue;
} // namespace base::trace_event


namespace subresource_filter {

// Encapsulates information about a version of unindexed subresource
// filtering rules on disk.
struct UnindexedRulesetInfo {
  UnindexedRulesetInfo();
  ~UnindexedRulesetInfo();
  UnindexedRulesetInfo(const UnindexedRulesetInfo&);
  UnindexedRulesetInfo& operator=(const UnindexedRulesetInfo&);

  // The version of the ruleset contents. Because the wire format of unindexed
  // rules is expected to be stable over time (at least backwards compatible),
  // the unindexed ruleset is uniquely identified by its content version.
  //
  // The version string must not be empty, but can be any string otherwise.
  // There is no ordering defined on versions.
  std::string content_version;

  // The (optional) path to the file containing the unindexed subresource
  // filtering rules. One (but not both) of |ruleset_path| and |resource_id|
  // should be set.
  base::FilePath ruleset_path;

  // The (optional) grit resource id containing the unindexed subresource
  // filtering rules, which if supplied is given to the ResourceBundle to
  // resolve to a string. One (but not both) of |ruleset_path| and |resource_id|
  // should be set.
  int resource_id = 0;

  // The (optional) path to a file containing the applicable license, which will
  // be copied next to the indexed ruleset. For convenience, the lack of license
  // can be indicated not only by setting |license_path| to empty, but also by
  // setting it to any non existent path.
  base::FilePath license_path;
};

// Encapsulates the combination of the binary format version of the indexed
// ruleset, and the version of the ruleset contents.
//
// In contrast to the unindexed ruleset, the binary format of the index data
// structures is expected to evolve over time, so the indexed ruleset is
// identified by a pair of versions: the content version of the rules that have
// been indexed; and the binary format version of the indexed data structures.
// It also contains a checksum of the data to ensure it hasn't been corrupted
// and a filter tag string to identify the type of filter the ruleset is used
// for as well as the names of prefs that store the current version.
struct IndexedRulesetVersion {
  explicit IndexedRulesetVersion(std::string_view filter_tag);
  IndexedRulesetVersion(std::string_view content_version,
                        int format_version,
                        std::string_view filter_tag);
  ~IndexedRulesetVersion();
  IndexedRulesetVersion& operator=(const IndexedRulesetVersion&);

  static void RegisterPrefs(PrefRegistrySimple* registry,
                            std::string_view filter_tag);
  // TODO(crbug.com/40280666): Change this function to consult multiple current
  // format versions once the rest of the ruleset pipeline has been refactored
  // to be generic.
  static int CurrentFormatVersion();

  bool IsValid() const;
  bool IsCurrentFormatVersion() const;

  void SaveToPrefs(PrefService* local_state) const;
  void ReadFromPrefs(PrefService* local_state);

  std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;

  std::string content_version;
  int format_version = 0;
  int checksum = 0;

  // Unique tag identifying the type of filter this IndexedRulesetVersion
  // is used for and thus what type of ruleset it corresponds to. Also used
  // as the prefix for pref names that store the current version.
  std::string filter_tag;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_VERSION_H_
