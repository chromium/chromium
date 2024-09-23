// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_
#define CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/prompt_behavior.h"
#include "chrome/test/chromedriver/session.h"

namespace base {
class CommandLine;
}

class Status;

class Switches {
 public:
  typedef base::FilePath::StringType NativeString;
  Switches();
  Switches(const Switches& other);
  ~Switches();

  void SetSwitch(const std::string& name);
  void SetSwitch(const std::string& name, const std::string& value);
  void SetSwitch(const std::string& name, const base::FilePath& value);

  void SetMultivaluedSwitch(const std::string& name, const std::string& value);

  // In case of same key, |switches| will override.
  void SetFromSwitches(const Switches& switches);

  // Sets a switch from the capabilities, of the form [--]name[=value].
  void SetUnparsedSwitch(const std::string& unparsed_switch);

  void RemoveSwitch(const std::string& name);

  bool HasSwitch(const std::string& name) const;
  std::string GetSwitchValue(const std::string& name) const;
  NativeString GetSwitchValueNative(const std::string& name) const;

  size_t GetSize() const;

  void AppendToCommandLine(base::CommandLine* command) const;
  std::string ToString() const;

 private:
  typedef std::map<std::string, NativeString> SwitchMap;
  SwitchMap switch_map_;
};

typedef std::map<std::string, Log::Level> LoggingPrefs;

struct PerfLoggingPrefs {
  PerfLoggingPrefs();
  ~PerfLoggingPrefs();

  // We must distinguish between a log domain being set by default and being
  // explicitly set. Otherwise, |PerformanceLogger| could only handle 3 of 4
  // possible combinations (tracing enabled/disabled + Timeline on/off).
  enum InspectorDomainStatus {
    kDefaultEnabled,
    kDefaultDisabled,
    kExplicitlyEnabled,
    kExplicitlyDisabled
  };

  InspectorDomainStatus network;
  InspectorDomainStatus page;

  std::string trace_categories;  // Non-empty string enables tracing.
  int buffer_usage_reporting_interval;  // ms between trace buffer usage events.
};

struct Capabilities {
  Capabilities();
  ~Capabilities();

  // Return true if remote host:port session is to be used.
  bool IsRemoteBrowser() const;

  // Return true if android package is specified.
  bool IsAndroid() const;

  // Accepts all W3C defined capabilities
  // and all ChromeDriver-specific extensions.
  Status Parse(const base::Value::Dict& desired_caps,
               bool w3c_compliant = true);

  //
  // W3C defined capabilities
  //

  bool accept_insecure_certs;

  std::string browser_name;
  std::string browser_version;
  std::string platform_name;

  std::string page_load_strategy;

  // Data from "proxy" capability are stored in "switches" field.

  base::TimeDelta script_timeout = Session::kDefaultScriptTimeout;
  base::TimeDelta page_load_timeout = Session::kDefaultPageLoadTimeout;
  base::TimeDelta implicit_wait_timeout = Session::kDefaultImplicitWaitTimeout;
  base::TimeDelta browser_startup_timeout =
      Session::kDefaultBrowserStartupTimeout;

  bool strict_file_interactability;

  std::optional<PromptBehavior> unhandled_prompt_behavior;

  //
  // ChromeDriver specific capabilities
  //

  std::string android_activity;

  std::string android_device_serial;

  std::string android_package;

  std::string android_process;

  std::string android_device_socket;

  std::string android_exec_name;

  bool android_use_running_app;

  bool android_keep_app_data_dir = false;

  int android_devtools_port = 0;

  base::FilePath binary;

  // If provided, the remote debugging address to connect to.
  NetAddress debugger_address;

  // Whether the lifetime of the started Chrome browser process should be
  // bound to ChromeDriver's process. If true, Chrome will not quit if
  // ChromeDriver dies.
  bool detach;

  std::optional<MobileDevice> mobile_device;

  // Set of switches which should be removed from default list when launching
  // Chrome.
  std::set<std::string> exclude_switches;

  std::vector<std::string> extensions;

  // Time to wait for extension background page to appear. If 0, no waiting.
  base::TimeDelta extension_load_timeout;

  std::unique_ptr<base::Value::Dict> local_state;

  std::string log_path;

  LoggingPrefs logging_prefs;

  // If set, enable minidump for chrome crashes and save to this directory.
  std::string minidump_path;

  bool network_emulation_enabled;

  PerfLoggingPrefs perf_logging_prefs;

  base::Value devtools_events_logging_prefs;

  std::unique_ptr<base::Value::Dict> prefs;

  Switches switches;

  std::set<WebViewInfo::Type> window_types;

  bool web_socket_url = false;
};

bool GetChromeOptionsDictionary(const base::Value::Dict& params,
                                const base::Value::Dict** out);

#endif  // CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_
