// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_TRACKER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_TRACKER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/default_session_controller.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {
class Clock;
class CommandLine;
}

namespace leveldb_proto {
class ProtoDatabaseProvider;
}

namespace feature_engagement {

class Configuration;
class Tracker;
class SessionController;

// Creates a Tracker that is usable for a demo mode.
std::unique_ptr<Tracker> CreateDemoModeTracker(std::string chosen_feature_name);

// A handle for the display lock. While this is unreleased, no in-product help
// can be displayed.
class DisplayLockHandle {
 public:
  typedef base::OnceClosure ReleaseCallback;
  explicit DisplayLockHandle(ReleaseCallback callback);

  DisplayLockHandle(const DisplayLockHandle&) = delete;
  DisplayLockHandle& operator=(const DisplayLockHandle&) = delete;

  ~DisplayLockHandle();

 private:
  ReleaseCallback release_callback_;
};

// A class that can export events from another tracking system to the Tracker so
// they can be migrated.
class TrackerEventExporter {
 public:
  // Struct to hold data about one event to migrate. |day| should be the number
  // of days since the UNIX epoch.
  struct EventData {
   public:
    std::string event_name;
    uint32_t day;

    EventData(std::string event_name, uint32_t day)
        : event_name(event_name), day(day) {}
  };

  virtual ~TrackerEventExporter() = default;

  // The tracker will call this once its own initialization has mostly completed
  // to ask for any new events to add.
  using ExportEventsCallback =
      base::OnceCallback<void(const std::vector<EventData> events)>;

  // Asks the class to load any events to export and provide them back to the
  // tracker via |callback|. |callback| must be called on the same thread that
  // this method was invoked on.
  virtual void ExportEvents(ExportEventsCallback callback) = 0;
};

// The Tracker provides a backend for displaying feature
// enlightenment or in-product help (IPH) with a clean and easy to use API to be
// consumed by the UI frontend. The backend behaves as a black box and takes
// input about user behavior. Whenever the frontend gives a trigger signal that
// IPH could be displayed, the backend will provide an answer to whether it is
// appropriate to show it or not.
class Tracker : public KeyedService, public base::SupportsUserData {
 public:
  // Describes the state of whether in-product helps has already been displayed
  // enough times or not within the bounds of the configuration for a
  // base::Feature. NOT_READY is returned if the Tracker has not been
  // initialized yet before the call to GetTriggerState(...).
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.feature_engagement
  enum class TriggerState : int {
    HAS_BEEN_DISPLAYED = 0,
    HAS_NOT_BEEN_DISPLAYED = 1,
    NOT_READY = 2
  };

  // Represents the action taken by the user on the snooze UI.
  // These enums are persisted as histogram entries, so this enum should be
  // treated as append-only and kept in sync with InProductHelpSnoozeAction in
  // enums.xml.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.feature_engagement
  enum class SnoozeAction : int {
    // User chose to snooze the IPH.
    SNOOZED = 1,
    // User chose to dismiss the IPH.
    DISMISSED = 2,
    // Constant used by the histogram macros.
    kMaxValue = DISMISSED
  };

  // Result of the backend query for whether or not to trigger any help UI.
  // A similar class will also be added to the java layer.
  struct TriggerDetails {
   public:
    TriggerDetails(bool should_trigger_iph, bool should_show_snooze);
    TriggerDetails(const TriggerDetails& trigger_details);
    ~TriggerDetails();

    // Whether or not to show the help UI.
    bool ShouldShowIph() const;

    // Whether to show a snooze option in the help UI.
    bool ShouldShowSnooze() const;

   private:
    bool should_trigger_iph_;
    bool should_show_snooze_;
  };

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type Tracker for the given Tracker.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      Tracker* feature_engagement);
#endif  // BUILDFLAG(IS_ANDROID)

  // Invoked when the tracker has been initialized. The |success| parameter
  // indicates that the initialization was a success and the tracker is ready to
  // receive calls.
  using OnInitializedCallback = base::OnceCallback<void(bool success)>;

  // The |storage_dir| is the path to where all local storage will be.
  // The |background_task_runner| will be used for all disk reads and writes.
  // If `configuration_providers` is not specified, a default set of providers
  // will be provided.
  static Tracker* Create(
      const base::FilePath& storage_dir,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      leveldb_proto::ProtoDatabaseProvider* db_provider,
      std::unique_ptr<TrackerEventExporter> event_exporter,
      const ConfigurationProviderList& configuration_providers =
          GetDefaultConfigurationProviders(),
      std::unique_ptr<SessionController> session_controller =
          std::make_unique<DefaultSessionController>());
  // Possibly adds a command line argument for a child browser process to
  // communicate what IPH are allowed in a testing environment. Has no effect if
  // IPH behavior is not being modified for testing. If specific IPH features
  // are explicitly allowed for the test, may add those to the --enable-features
  // command line parameter as well (will add it if not present).
  static void PropagateTestStateToChildProcess(base::CommandLine& command_line);

  Tracker(const Tracker&) = delete;
  Tracker& operator=(const Tracker&) = delete;

  // Must be called whenever an event happens.
  virtual void NotifyEvent(const std::string& event) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Notifies that the "used" event for `feature` has happened.
  virtual void NotifyUsedEvent(const base::Feature& feature) = 0;

  // Erases all event data associated with a particular `feature`, including -
  // but not limited to - trigger and used event data.
  //
  // This method is used by specific internals and test code.
  virtual void ClearEventData(const base::Feature& feature) = 0;

  // Retrieves information about each event condition and event count associated
  // with a feature. The count will reflect the time window in EventConfig.
  using EventList = std::vector<std::pair<EventConfig, int>>;
  virtual EventList ListEvents(const base::Feature& feature) const = 0;
#endif

  // This function must be called whenever the triggering condition for a
  // specific feature happens. Returns true iff the display of the in-product
  // help must happen.
  // If |true| is returned, the caller *must* call Dismissed(...) when display
  // of feature enlightenment ends.
  [[nodiscard]] virtual bool ShouldTriggerHelpUI(
      const base::Feature& feature) = 0;

  // For callers interested in showing a snooze button. For other callers, use
  // the ShouldTriggerHelpUI(..) method.
  virtual TriggerDetails ShouldTriggerHelpUIWithSnooze(
      const base::Feature& feature) = 0;

  // Invoking this is basically the same as being allowed to invoke
  // ShouldTriggerHelpUI(...) without requiring to show the in-product help.
  // This function may be called to inspect if the current state would allow the
  // given |feature| to pass all its conditions and display the feature
  // enlightenment.
  //
  // NOTE: It is still required to invoke ShouldTriggerHelpUI(...) if feature
  // enlightenment should be shown.
  //
  // NOTE: It is not guaranteed that invoking ShouldTriggerHelpUI(...)
  // after this would yield the same result. The state might change
  // in-between the calls because time has passed, other events might have been
  // triggered, and other state might have changed.
  virtual bool WouldTriggerHelpUI(const base::Feature& feature) const = 0;

  // This function can be called to query if a particular |feature| has ever
  // been displayed at least once in the past. The days counted is controlled by
  // the EventConfig of "event_trigger". If |from_window| is set to true, the
  // search window size will be set to event_trigger.window; otherwise, the
  // window size will be event_trigger.storage.
  //
  // Calling this method requires the Tracker to already have been initialized.
  // See IsInitialized() and AddOnInitializedCallback(...) for how to ensure
  // the call to this is delayed.
  virtual bool HasEverTriggered(const base::Feature& feature,
                                bool from_window) const = 0;

  // This function can be called to query if a particular |feature| meets its
  // particular precondition for triggering within the bounds of the current
  // feature configuration.
  // Calling this method requires the Tracker to already have been initialized.
  // See IsInitialized() and AddOnInitializedCallback(...) for how to ensure
  // the call to this is delayed.
  // This function can typically be used to ensure that expensive operations
  // for tracking other state related to in-product help do not happen if
  // in-product help has already been displayed for the given |feature|.
  virtual TriggerState GetTriggerState(const base::Feature& feature) const = 0;

  // Must be called after display of feature enlightenment finishes for a
  // particular |feature|.
  virtual void Dismissed(const base::Feature& feature) = 0;

  // For callers interested in showing a snooze button. For other callers, use
  // the Dismissed(..) method.
  virtual void DismissedWithSnooze(
      const base::Feature& feature,
      std::optional<SnoozeAction> snooze_action) = 0;

  // Acquiring a display lock means that no in-product help can be displayed
  // while it is held. To release the lock, delete the handle.
  // If in-product help is already displayed while the display lock is
  // acquired, the lock is still handed out, but it will not dismiss the current
  // in-product help. However, no new in-product help will be shown until all
  // locks have been released. It is required to release the DisplayLockHandle
  // once the lock should no longer be held.
  // The DisplayLockHandle must be released on the main thread.
  // This method returns nullptr if no handle could be retrieved.
  virtual std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() = 0;

  // Called by the client to notify the tracker that a priority notification
  // should be shown. If a handler has already been registered, the IPH will be
  // shown right away. Otherwise, the tracker will cache the priority feature
  // and will show the IPH whenever a handler is registered in future. All other
  // IPHs will be blocked until then. It isn't allowed to invoke this method
  // again with another notification before the existing one is processed.
  virtual void SetPriorityNotification(const base::Feature& feature) = 0;

  // Called to get if there is a pending priority notification to be shown next.
  virtual std::optional<std::string> GetPendingPriorityNotification() = 0;

  // Called by the client to register a handler for priority notifications. This
  // will essentially contain the code to spin up an IPH.
  virtual void RegisterPriorityNotificationHandler(
      const base::Feature& feature,
      base::OnceClosure callback) = 0;

  // Unregister the handler. Must be called during client destruction.
  virtual void UnregisterPriorityNotificationHandler(
      const base::Feature& feature) = 0;

  // Returns whether the tracker has been successfully initialized. During
  // startup, this will be false until the internal models have been loaded at
  // which point it is set to true if the initialization was successful. The
  // state will never change from initialized to uninitialized.
  // Callers can invoke AddOnInitializedCallback(...) to be notified when the
  // result of the initialization is ready.
  virtual bool IsInitialized() const = 0;

  // For features that trigger on startup, they can register a callback to
  // ensure that they are informed when the tracker has finished the
  // initialization. If the tracker has already been initialized, the callback
  // will still be invoked with the result. The callback is guaranteed to be
  // invoked exactly one time.
  virtual void AddOnInitializedCallback(OnInitializedCallback callback) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Updates the config of a specific feature after initialization. The new
  // config will replace the existing cofig.
  // Calling this method requires the Tracker to already have been initialized.
  // See IsInitialized() and AddOnInitializedCallback(...) for how to ensure
  // the call to this is delayed.
  virtual void UpdateConfig(const base::Feature& feature,
                            const ConfigurationProvider* provider) = 0;
#endif

  // Returns the configuration associated with the tracker for testing purposes.
  virtual const Configuration* GetConfigurationForTesting() const = 0;

  // Set a testing clock for the tracker. It's recommended to use a
  // SimpleTestClock, so we can advacne the clock in test.
  virtual void SetClockForTesting(const base::Clock& clock,
                                  base::Time initial_now) = 0;

  // Returns the default set of configuration providers.
  static ConfigurationProviderList GetDefaultConfigurationProviders();

 protected:
  Tracker() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_TRACKER_H_
