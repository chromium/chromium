// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_METRICS_LOGIN_EVENT_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_METRICS_LOGIN_EVENT_RECORDER_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
}

namespace ash {

// Names of UMA prefixes that are used to differentiate between different
// WriteTimes() operations.
constexpr char kUmaLoginPrefix[] = "BootTime.";
constexpr char kUmaLogoutPrefix[] = "ShutdownTime.";

// LoginEventRecorder is a utility class used to save the bootimes of Chrome OS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_METRICS) LoginEventRecorder {
 public:
  class TimeMarker {
   public:
    TimeMarker(const char* name,
               std::optional<std::string> url,
               bool send_to_uma,
               bool write_to_file);
    TimeMarker(const TimeMarker& other);
    ~TimeMarker();

    const char* name() const { return name_; }
    base::TimeTicks time() const { return time_; }
    const std::optional<std::string>& url() const { return url_; }
    bool send_to_uma() const { return send_to_uma_; }
    bool write_to_file() const { return write_to_file_; }

    // comparator for sorting
    bool operator<(const TimeMarker& other) const {
      return time_ < other.time_;
    }

   private:
    friend class std::vector<TimeMarker>;
    const char* name_;
    base::TimeTicks time_ = base::TimeTicks::Now();
    std::optional<std::string> url_;
    bool send_to_uma_;
    bool write_to_file_;
  };

  class Stats {
   public:
    // Initializes stats with current /proc values.
    static Stats GetCurrentStats();

    // Returns JSON representation.
    std::string SerializeToString() const;

    // Creates new object from JSON representation.
    static Stats DeserializeFromString(const std::string& value);

    const std::string& uptime() const { return uptime_; }
    const std::string& disk() const { return disk_; }

    // Writes "uptime in seconds" to result. (This is first field in uptime_.)
    // Returns true on successful conversion.
    bool UptimeDouble(double* result) const;

    // Stores stats to 'type-name' file with the given |name|.
    // I.e. '/run/bootstat/uptime-logout-started' and
    // '/run/bootstat/disk-logout-started' for name='logout-started'.
    //
    // When |write_flag_file| is true, also creates 'stats-name.written' flag
    // file to signal that stats were appended.
    // I.e. '/tmp/stats-logout-started.written' for name='logout-started'.
    void RecordStats(const std::string& name, bool write_flag_file) const;
    void RecordStatsWithCallback(const std::string& name,
                                 bool write_flag_file,
                                 base::OnceClosure callback) const;

   private:
    // Runs asynchronously when RecordStats(WithCallback) is called.
    void RecordStatsAsync(const std::string& name, bool write_flag_file) const;

    std::string uptime_;
    std::string disk_;
  };

  LoginEventRecorder();
  LoginEventRecorder(const LoginEventRecorder&) = delete;
  LoginEventRecorder& operator=(const LoginEventRecorder&) = delete;
  virtual ~LoginEventRecorder();

  static LoginEventRecorder* Get();

  // Add a time marker for login. A timeline will be dumped to
  // /tmp/login-times-sent after login is done. If |send_to_uma| is true
  // the time between this marker and the last will be sent to UMA with
  // the identifier BootTime.|marker_name|.
  void AddLoginTimeMarker(const char* marker_name,
                          bool send_to_uma,
                          bool write_to_file = true);

  void AddLoginTimeMarkerWithURL(const char* marker_name,
                                 std::optional<std::string> url,
                                 bool send_to_uma,
                                 bool write_to_file = true);

  // Add a time marker for logout. A timeline will be dumped to
  // /tmp/logout-times-sent after logout is done. If |send_to_uma| is true
  // the time between this marker and the last will be sent to UMA with
  // the identifier ShutdownTime.|marker_name|.
  void AddLogoutTimeMarker(const char* marker_name, bool send_to_uma);

  // Record events for successful authentication.
  void RecordAuthenticationSuccess();

  // Record events for authentication failure.
  void RecordAuthenticationFailure();

  void ClearLoginTimeMarkers();

  // Records current uptime and disk usage for metrics use.
  // Posts task to file thread.
  // name will be used as part of file names in /tmp.
  // Existing stats files will not be overwritten.
  void RecordCurrentStats(const std::string& name);

  // Schedule writing login times to logs.
  void ScheduleWriteLoginTimes(const std::string base_name,
                               const std::string uma_name,
                               const std::string uma_prefix);
  // Immediately execute the task to write login times.
  void RunScheduledWriteLoginTimes();

  // Returns the duration between given markers.
  std::optional<base::TimeDelta> GetDuration(
      const std::string& begin_marker_name,
      const std::string& end_marker_name);

  // Write logout times to logs.
  void WriteLogoutTimes(const std::string base_name,
                        const std::string uma_name,
                        const std::string uma_prefix);

  // Stores a copy of the events to be retrieved by tests.
  // Also all future events will have stored copies for testing.
  void PrepareEventCollectionForTesting();

  // Returns list of all events collected.
  const std::vector<TimeMarker>& GetCollectedLoginEventsForTesting();

 private:
  void AddMarker(std::vector<TimeMarker>* vector, TimeMarker&& marker);

  void WriteLoginTimesDelayed();

  std::vector<TimeMarker> login_time_markers_;
  std::vector<TimeMarker> logout_time_markers_;
  base::OnceCallback<void(std::vector<TimeMarker>)> callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // This list is never cleared. It has copy of all the login events that
  // login_time_markers_ had when PrepareEventCollectionForTesting() was
  // called and all login avents since that moment.
  std::optional<std::vector<TimeMarker>> login_time_markers_for_testing_;

  base::WeakPtrFactory<LoginEventRecorder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_METRICS_LOGIN_EVENT_RECORDER_H_
