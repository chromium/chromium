// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RLZ_RLZ_TRACKER_H_
#define COMPONENTS_RLZ_RLZ_TRACKER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "rlz/lib/rlz_lib.h"

namespace base {
class SequencedTaskRunner;
}

namespace rlz {

class RLZTrackerDelegate;

// RLZ is a library which is used to measure distribution scenarios.
// Its job is to record certain lifetime events in the registry and to send
// them encoded as a compact string at most twice. The sent data does
// not contain information that can be used to identify a user or to infer
// browsing habits. The API in this file is a wrapper around the open source
// RLZ library which can be found at http://code.google.com/p/rlz.
//
// For partner or bundled installs, the RLZ might send more information
// according to the terms disclosed in the EULA.

class RLZTracker {
 public:
  RLZTracker(const RLZTracker&) = delete;
  RLZTracker& operator=(const RLZTracker&) = delete;

  // Sets the RLZTrackerDelegate that should be used by the global RLZTracker
  // instance. Must be called before calling any other method of RLZTracker.
  static void SetRlzDelegate(std::unique_ptr<RLZTrackerDelegate> delegate);

  // Clear the RLZTrackerDelegate for testing only.
  static void ClearRlzDelegateForTesting();

  // Initializes the RLZ library services for use in chrome. Schedules a delayed
  // task that performs the ping and registers some events when 'first-run' is
  // true.
  //
  // When |send_ping_immediately| is true, a financial ping should be sent
  // immediately after a first search is recorded, without waiting for |delay|.
  // However, we only want this behaviour on first run.
  //
  // If the chrome brand is organic (no partners) then the pings don't occur.
  static bool InitRlzDelayed(bool first_run,
                             bool send_ping_immediately,
                             base::TimeDelta delay,
                             bool is_google_default_search,
                             bool is_google_homepage,
                             bool is_google_in_startpages);

  // Records an RLZ event. Some events can be access point independent.
  // Returns false if the event could not be recorded. Requires write access
  // to the HKCU registry hive on windows.
  static bool RecordProductEvent(rlz_lib::Product product,
                                 rlz_lib::AccessPoint point,
                                 rlz_lib::Event event_id);

  // For the point parameter of RecordProductEvent.
  static rlz_lib::AccessPoint ChromeOmnibox();
#if !BUILDFLAG(IS_IOS)
  static rlz_lib::AccessPoint ChromeHomePage();
  static rlz_lib::AccessPoint ChromeAppList();
#endif  // !BUILDFLAG(IS_IOS)

  // Gets the HTTP header value that can be added to requests from the
  // specific access point.  The string returned is of the form:
  //
  //    "X-Rlz-String: <access-point-rlz>\r\n"
  //
  static std::string GetAccessPointHttpHeader(rlz_lib::AccessPoint point);

  // Gets the RLZ value of the access point.
  // Returns false if the rlz string could not be obtained. In some cases
  // an empty string can be returned which is not an error.
  static bool GetAccessPointRlz(rlz_lib::AccessPoint point,
                                std::u16string* rlz);

  // Invoked during shutdown to clean up any state created by RLZTracker.
  static void CleanupRlz();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Clears all product state. Should be called when turning RLZ off. On other
  // platforms, this is done by product uninstaller.
  static void ClearRlzState();
#endif

  // This method is public for use by the Singleton class.
  static RLZTracker* GetInstance();

  // Enables zero delay for InitRlzDelayed. For testing only.
  static void EnableZeroDelayForTesting();

#if !BUILDFLAG(IS_IOS)
  // Records that the app list search has been used.
  static void RecordAppListSearch();

  // Returns true if there is a non-organic brand code and we have never
  // recorded that user has performed a Google search from their Google homepage
  // yet.
  static bool ShouldRecordChromeHomePageSearch();

  // Records that the user has their homepage set to Google search and performs
  // a Google search from there. This event should be recorded at most once.
  static void RecordChromeHomePageSearch();

  // Manually sets if the search has been performed for testing only.
  static void SetRlzChromeHomePageSearchRecordedForTesting(bool recorded);
#endif  // !BUILDFLAG(IS_IOS)

  // The following methods are made protected so that they can be used for
  // testing purposes. Production code should never need to call these.
 protected:
  RLZTracker();
  virtual ~RLZTracker();

  // Performs initialization of RLZ tracker that is purposefully delayed so
  // that it does not interfere with chrome startup time.
  virtual void DelayedInit();

  // Used by test code to override the default RLZTracker instance returned
  // by GetInstance().
  void set_tracker(RLZTracker* tracker) { tracker_ = tracker; }

  // Sends the financial ping to the RLZ servers and invalidates the RLZ string
  // cache since the response from the RLZ server may have changed then.
  // Protected so that its accessible from tests.
  void PingNowImpl();

 private:
  friend class base::NoDestructor<RLZTracker>;
  friend class base::RefCountedThreadSafe<RLZTracker>;

  // Implementation called from SetRlzDelegate() static method.
  void SetDelegate(std::unique_ptr<RLZTrackerDelegate> delegate);

  // Implementation called from ClearRlzDelegateForTesting() static method.
  void ClearDelegateForTesting();

  // Implementation called from InitRlzDelayed() static method.
  bool Init(bool first_run,
            bool send_ping_immediately,
            base::TimeDelta delay,
            bool is_google_default_search,
            bool is_google_homepage,
            bool is_google_in_startpages);

  // Implementation called from CleanupRlz static method.
  void Cleanup();

  // Implementation called from RecordProductEvent() static method.
  bool RecordProductEventImpl(rlz_lib::Product product,
                              rlz_lib::AccessPoint point,
                              rlz_lib::Event event_id);

  // Records FIRST_SEARCH event. Passed as bound callback to RLZTrackerDelegate.
  void RecordFirstSearch(rlz_lib::AccessPoint point);

  // Implementation called from GetAccessPointRlz() static method.
  bool GetAccessPointRlzImpl(rlz_lib::AccessPoint point, std::u16string* rlz);

  // Schedules the delayed initialization. This method is virtual to allow
  // tests to override how the scheduling is done.
  virtual void ScheduleDelayedInit(base::TimeDelta delay);

  // Schedules a call to rlz_lib::RecordProductEvent(). This method is virtual
  // to allow tests to override how the scheduling is done.
  virtual bool ScheduleRecordProductEvent(rlz_lib::Product product,
                                          rlz_lib::AccessPoint point,
                                          rlz_lib::Event event_id);

  // Schedules a call to rlz_lib::RecordFirstSearch(). This method is virtual
  // to allow tests to override how the scheduling is done.
  virtual bool ScheduleRecordFirstSearch(rlz_lib::AccessPoint point);

  // Schedules a call to rlz_lib::SendFinancialPing(). This method is virtual
  // to allow tests to override how the scheduling is done.
  virtual void ScheduleFinancialPing();

  // Schedules a call to GetAccessPointRlz() on the I/O thread if the current
  // thread is not already the I/O thread, otherwise does nothing. Returns
  // true if the call was scheduled, and false otherwise. This method is
  // virtual to allow tests to override how the scheduling is done.
  virtual bool ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point);

  // Sends the financial ping to the RLZ servers. This method is virtual to
  // allow tests to override.
  virtual bool SendFinancialPing(const std::string& brand,
                                 const std::u16string& lang,
                                 const std::u16string& referral);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Implementation called from ClearRlzState static method.
  void ClearRlzStateImpl();

  // Schedules a call to ClearRlzStateImpl(). This method is virtual
  // to allow tests to override how the scheduling is done.
  virtual bool ScheduleClearRlzState();
#endif

  // Returns a pointer to the bool corresponding to whether |point| has been
  // used but not reported.
  bool* GetAccessPointRecord(rlz_lib::AccessPoint point);

#if !BUILDFLAG(IS_IOS)
  // Implementation called from SetRlzChromeHomePageSearchRecordedForTesting()
  // static method.
  void SetChromeHomePageSearchRecordedForTesting(bool recorded);
#endif  // !BUILDFLAG(IS_IOS)

  // Tracker used for testing purposes only. If this value is non-NULL, it
  // will be returned from GetInstance() instead of the regular singleton.
  static RLZTracker* tracker_;

  // Delegate abstracting embedder specific knowledge. Must not be null.
  std::unique_ptr<RLZTrackerDelegate> delegate_;

  // Configuation data for RLZ tracker. Set by call to Init().
  bool first_run_;
  bool send_ping_immediately_;
  bool is_google_default_search_;
  bool is_google_homepage_;
  bool is_google_in_startpages_;

  // Keeps track if the RLZ tracker has already performed its delayed
  // initialization.
  bool already_ran_;

  // Keeps a cache of RLZ access point strings, since they rarely change.
  // The cache must be protected by a lock since it may be accessed from
  // the UI thread for reading and the IO thread for reading and/or writing.
  base::Lock cache_lock_;
  std::map<rlz_lib::AccessPoint, std::u16string> rlz_cache_
      GUARDED_BY(cache_lock_);

  // Keeps track of whether the omnibox, home page or app list have been used.
  bool omnibox_used_;
  bool homepage_used_;
  bool app_list_used_;

#if !BUILDFLAG(IS_IOS)
  // Sets to true when we have attempted to record that user has performed a
  // Google search from their Google homepage. This will be set to true
  // regardless whether the event is recorded successfully, so that new
  // |ChromeRLZTrackerWebContentsObserver| only observes web contents if still
  // needed. On the contrast, |homepage_used_| is only set to true if the event
  // is not recorded successfully and needs another attempt.
  bool chrome_homepage_search_recorded_ = false;
#endif  // !BUILDFLAG(IS_IOS)

  // Main and (optionally) reactivation brand codes, assigned on UI thread.
  std::string brand_;
  std::string reactivation_brand_;

  // Minimum delay before sending financial ping after initialization.
  base::TimeDelta min_init_delay_;

  class WrapperURLLoaderFactory;
  std::unique_ptr<WrapperURLLoaderFactory> custom_url_loader_factory_;

  // Runner for RLZ background tasks.  The checker is used to verify operations
  // occur in the correct sequence, especially in tests.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace rlz

#endif  // COMPONENTS_RLZ_RLZ_TRACKER_H_
