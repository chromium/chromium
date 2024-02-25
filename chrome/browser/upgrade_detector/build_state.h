// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_BUILD_STATE_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_BUILD_STATE_H_

#include <optional>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/version.h"

class BuildStateObserver;

// The state of the browser or device build. This class is not thread safe --
// all access must take place on the UI thread.
class BuildState {
 public:
  // Logged as BuildStateUpdateType. Do not reorder or remove entries.
  enum class UpdateType {
    // No new version ready for use.
    kNone = 0,

    // A bump from the running version to a newer one (i.e., a typical update).
    kNormalUpdate = 1,

    // Rollback to an older version via administrative action.
    kEnterpriseRollback = 2,

    // Rollback to an older version via a switch to a more stable channel.
    // (Chrome OS only.)
    kChannelSwitchRollback = 3,

    // Needed for UMA - can be removed if BuildStateUpdateType is removed.
    kMaxValue = kChannelSwitchRollback,
  };

  BuildState();
  BuildState(const BuildState&) = delete;
  BuildState& operator=(const BuildState&) = delete;
  ~BuildState();

  // Returns the current update type if an update has been detected, or kNone if
  // one has not been detected. In the latter case, neither installed_version()
  // nor critical_version() is relevant.
  UpdateType update_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return update_type_;
  }

  // If update_type() is not kNone, returns the discovered version or no value
  // if an error occurred while determining the installed version. A returned
  // value may be numerically higher or lower than the currently running build.
  // Note: On Chrome OS, this is the system version number rather than the
  // browser version number.
  const std::optional<base::Version>& installed_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return installed_version_;
  }

  // If update_type() is not kNone, returns the optional critical version,
  // indicating a minimum version that must be running. A running version lower
  // than this must be updated as soon as possible. (Windows only.)
  const std::optional<base::Version>& critical_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return critical_version_;
  }

  // Sets the update properties. Observers are notified if the given properties
  // differ from the instance's previous properties.
  void SetUpdate(UpdateType update_type,
                 const base::Version& installed_version,
                 const std::optional<base::Version>& critical_version);

  void AddObserver(BuildStateObserver* observer);
  void RemoveObserver(const BuildStateObserver* observer);
  bool HasObserver(const BuildStateObserver* observer) const;

 private:
  void NotifyObserversOnUpdate();

  SEQUENCE_CHECKER(sequence_checker_);
  base::ObserverList<BuildStateObserver, /*check_empty=*/true> observers_;
  UpdateType update_type_ = UpdateType::kNone;
  std::optional<base::Version> installed_version_;
  std::optional<base::Version> critical_version_;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_BUILD_STATE_H_
