// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_POLLER_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_POLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/upgrade_detector/get_installed_version.h"

class BuildState;
class InstalledVersionMonitor;

namespace base {
class TickClock;
}  // namespace base

// Polls for the installed version of the browser in the background every two
// hours. Polling begins automatically after construction and continues until
// destruction. Modifications to the browser's install directory trigger a
// premature poll. The discovered version is provided to the given BuildState on
// each poll.
class InstalledVersionPoller {
 public:
  // The default polling interval. This may be overridden for testing by
  // specifying some number of seconds via --check-for-update-interval=seconds.
  static const base::TimeDelta kDefaultPollingInterval;

  // Constructs an instance that will poll upon construction and periodically
  // thereafter.
  explicit InstalledVersionPoller(BuildState* build_state);

  // A type of callback that is run (in the background) to get the currently
  // installed version of the browser.
  using GetInstalledVersionCallback =
      base::RepeatingCallback<void(InstalledVersionCallback)>;

  // A constructor for tests that provide a mock source of time and a mock
  // version getter.
  InstalledVersionPoller(BuildState* build_state,
                         GetInstalledVersionCallback get_installed_version,
                         std::unique_ptr<InstalledVersionMonitor> monitor,
                         const base::TickClock* tick_clock);
  InstalledVersionPoller(const InstalledVersionPoller&) = delete;
  InstalledVersionPoller& operator=(const InstalledVersionPoller&) = delete;
  ~InstalledVersionPoller();

  class ScopedDisableForTesting {
   public:
    ScopedDisableForTesting(ScopedDisableForTesting&&) noexcept = default;
    ScopedDisableForTesting& operator=(ScopedDisableForTesting&&) = default;
    ~ScopedDisableForTesting();

   private:
    friend class InstalledVersionPoller;
    ScopedDisableForTesting();
  };

  // Returns an object that prevents any newly created instances of
  // InstalledVersionPoller from doing any work for the duration of its
  // lifetime.
  static ScopedDisableForTesting MakeScopedDisableForTesting() {
    return ScopedDisableForTesting();
  }

 private:
  enum class PollType;

  // Starts observing changes to the browser's installation.
  void StartMonitor(std::unique_ptr<InstalledVersionMonitor> monitor);

  // Handles the result of a change in the browser's installation. If |error| is
  // false, a task will be scheduled to poll the installed version in the
  // background. Otherwise, if |error| is true, an error has occurred and no
  // further changes will be monitored.
  void OnMonitorResult(bool error);

  // Initiates a poll in the background.
  void Poll(PollType poll_type);

  // Handles the result of a poll. |poll_type| indicates the reason for the poll
  // (used for metrics), and |installed_version| contains the discovered version
  // information.
  void OnInstalledVersion(PollType poll_type,
                          InstalledAndCriticalVersion installed_version);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<BuildState> const build_state_;
  const GetInstalledVersionCallback get_installed_version_;
  base::OneShotTimer timer_;

  // Valid while observing modifications to the installation.
  std::unique_ptr<InstalledVersionMonitor> monitor_;

  base::WeakPtrFactory<InstalledVersionPoller> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_POLLER_H_
