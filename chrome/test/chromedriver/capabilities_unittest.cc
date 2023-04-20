// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/capabilities.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/strings/pattern.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Switches, Empty) {
  Switches switches;
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_EQ(0u, cmd.GetSwitches().size());
  ASSERT_EQ("", switches.ToString());
}

TEST(Switches, NoValue) {
  Switches switches;
  switches.SetSwitch("hello");

  ASSERT_TRUE(switches.HasSwitch("hello"));
  ASSERT_EQ("", switches.GetSwitchValue("hello"));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL(""), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello", switches.ToString());
}

TEST(Switches, Value) {
  Switches switches;
  switches.SetSwitch("hello", "there");

  ASSERT_TRUE(switches.HasSwitch("hello"));
  ASSERT_EQ("there", switches.GetSwitchValue("hello"));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL("there"), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello=there", switches.ToString());
}

TEST(Switches, FromOther) {
  Switches switches;
  switches.SetSwitch("a", "1");
  switches.SetSwitch("b", "1");

  Switches switches2;
  switches2.SetSwitch("b", "2");
  switches2.SetSwitch("c", "2");

  switches.SetFromSwitches(switches2);
  ASSERT_EQ("--a=1 --b=2 --c=2", switches.ToString());
}

TEST(Switches, Remove) {
  Switches switches;
  switches.SetSwitch("a", "1");
  switches.RemoveSwitch("a");
  ASSERT_FALSE(switches.HasSwitch("a"));
}

TEST(Switches, Quoting) {
  Switches switches;
  switches.SetSwitch("hello", "a  b");
  switches.SetSwitch("hello2", "  '\"  ");

  ASSERT_EQ("--hello=\"a  b\" --hello2=\"  '\\\"  \"", switches.ToString());
}

TEST(Switches, Multiple) {
  Switches switches;
  switches.SetSwitch("switch");
  switches.SetSwitch("hello", "there");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("switch"));
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL("there"), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello=there --switch", switches.ToString());
}

TEST(Switches, Unparsed) {
  Switches switches;
  switches.SetUnparsedSwitch("a");
  switches.SetUnparsedSwitch("--b");
  switches.SetUnparsedSwitch("--c=1");
  switches.SetUnparsedSwitch("d=1");
  switches.SetUnparsedSwitch("-e=--1=1");

  ASSERT_EQ("---e=--1=1 --a --b --c=1 --d=1", switches.ToString());
}

TEST(ParseCapabilities, UnknownCapabilityLegacy) {
  // In legacy mode, unknown capabilities are ignored.
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("foo", "bar");
  Status status = capabilities.Parse(caps, false);
  ASSERT_TRUE(status.IsOk());
}

TEST(ParseCapabilities, UnknownCapabilityW3c) {
  // In W3C mode, unknown capabilities results in error.
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("foo", "bar");
  Status status = capabilities.Parse(caps);
  ASSERT_EQ(status.code(), kInvalidArgument);
}

TEST(ParseCapabilities, WithAndroidPackage) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.androidPackage", "abc");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsAndroid());
  ASSERT_EQ("abc", capabilities.android_package);
}

TEST(ParseCapabilities, EmptyAndroidPackage) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.androidPackage", std::string());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalAndroidPackage) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.androidPackage", 123);
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, LogPath) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.logPath", "path/to/logfile");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_STREQ("path/to/logfile", capabilities.log_path.c_str());
}

TEST(ParseCapabilities, Args) {
  Capabilities capabilities;
  base::Value::List args;
  args.Append("arg1");
  args.Append("arg2=invalid");
  args.Append("arg2=val");
  args.Append("enable-blink-features=val1");
  args.Append("enable-blink-features=val2,");
  args.Append("--enable-blink-features=val3");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.args", std::move(args));

  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(3u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("arg1"));
  ASSERT_TRUE(capabilities.switches.HasSwitch("arg2"));
  ASSERT_EQ("", capabilities.switches.GetSwitchValue("arg1"));
  ASSERT_EQ("val", capabilities.switches.GetSwitchValue("arg2"));
  ASSERT_EQ("val1,val2,val3",
            capabilities.switches.GetSwitchValue("enable-blink-features"));
}

TEST(ParseCapabilities, Prefs) {
  Capabilities capabilities;
  base::Value::Dict prefs;
  prefs.Set("key1", "value1");
  prefs.SetByDottedPath("key2.k", "value2");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.prefs", prefs.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(*capabilities.prefs == prefs);
}

TEST(ParseCapabilities, LocalState) {
  Capabilities capabilities;
  base::Value::Dict local_state;
  local_state.Set("s1", "v1");
  local_state.SetByDottedPath("s2.s", "v2");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.localState", local_state.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(*capabilities.local_state == local_state);
}

TEST(ParseCapabilities, Extensions) {
  Capabilities capabilities;
  base::Value::List extensions;
  extensions.Append("ext1");
  extensions.Append("ext2");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.extensions",
                       base::Value(std::move(extensions)));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.extensions.size());
  ASSERT_EQ("ext1", capabilities.extensions[0]);
  ASSERT_EQ("ext2", capabilities.extensions[1]);
}

TEST(ParseCapabilities, UnrecognizedProxyType) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "unknown proxy type");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalProxyType) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", 123);
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, DirectProxy) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "direct");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("no-proxy-server"));
}

TEST(ParseCapabilities, SystemProxy) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "system");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(0u, capabilities.switches.GetSize());
}

TEST(ParseCapabilities, PacProxy) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "pac");
  proxy.Set("proxyAutoconfigUrl", "test.wpad");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_EQ("test.wpad", capabilities.switches.GetSwitchValue("proxy-pac-url"));
}

TEST(ParseCapabilities, MissingProxyAutoconfigUrl) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "pac");
  proxy.Set("httpProxy", "http://localhost:8001");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, AutodetectProxy) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "autodetect");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("proxy-auto-detect"));
}

TEST(ParseCapabilities, ManualProxy) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "manual");
  proxy.Set("ftpProxy", "localhost:9001");
  proxy.Set("httpProxy", "localhost:8001");
  proxy.Set("sslProxy", "localhost:10001");
  proxy.Set("socksProxy", "localhost:12345");
  proxy.Set("socksVersion", 5);
  base::Value::List bypass;
  bypass.Append("google.com");
  bypass.Append("youtube.com");
  proxy.Set("noProxy", std::move(bypass));
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.switches.GetSize());
  ASSERT_EQ(
      "ftp=localhost:9001;http=localhost:8001;https=localhost:10001;"
      "socks=socks5://localhost:12345",
      capabilities.switches.GetSwitchValue("proxy-server"));
  ASSERT_EQ(
      "google.com,youtube.com",
      capabilities.switches.GetSwitchValue("proxy-bypass-list"));
}

TEST(ParseCapabilities, IgnoreNullValueForManualProxy) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "manual");
  proxy.Set("ftpProxy", "localhost:9001");
  proxy.Set("sslProxy", base::Value());
  proxy.Set("noProxy", base::Value());
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("proxy-server"));
  ASSERT_EQ(
      "ftp=localhost:9001",
      capabilities.switches.GetSwitchValue("proxy-server"));
}

TEST(ParseCapabilities, MissingSocksVersion) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "manual");
  proxy.Set("socksProxy", "localhost:6000");
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, BadSocksVersion) {
  Capabilities capabilities;
  base::Value::Dict proxy;
  proxy.Set("proxyType", "manual");
  proxy.Set("socksProxy", "localhost:6000");
  proxy.Set("socksVersion", 256);
  base::Value::Dict caps;
  caps.Set("proxy", std::move(proxy));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, AcceptInsecureCertsDisabledByDefault) {
  Capabilities capabilities;
  base::Value::Dict caps;
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_FALSE(capabilities.accept_insecure_certs);
}

TEST(ParseCapabilities, EnableAcceptInsecureCerts) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("acceptInsecureCerts", true);
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.accept_insecure_certs);
}

TEST(ParseCapabilities, LoggingPrefsOk) {
  Capabilities capabilities;
  base::Value::Dict logging_prefs;
  logging_prefs.Set("Network", "INFO");
  base::Value::Dict caps;
  caps.Set("goog:loggingPrefs", std::move(logging_prefs));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.logging_prefs.size());
  ASSERT_EQ(Log::kInfo, capabilities.logging_prefs["Network"]);
}

TEST(ParseCapabilities, LoggingPrefsNotDict) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("goog:loggingPrefs", "INFO");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsInspectorDomainStatus) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::Value::Dict logging_prefs;
  logging_prefs.Set(WebDriverLog::kPerformanceType, "INFO");
  base::Value::Dict desired_caps;
  desired_caps.Set("goog:loggingPrefs", std::move(logging_prefs));
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            capabilities.perf_logging_prefs.network);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            capabilities.perf_logging_prefs.page);
  base::Value::Dict perf_logging_prefs;
  perf_logging_prefs.Set("enableNetwork", true);
  perf_logging_prefs.Set("enablePage", false);
  desired_caps.SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                               std::move(perf_logging_prefs));
  Status status = capabilities.Parse(desired_caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled,
            capabilities.perf_logging_prefs.network);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyDisabled,
            capabilities.perf_logging_prefs.page);
}

TEST(ParseCapabilities, PerfLoggingPrefsTracing) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::Value::Dict logging_prefs;
  logging_prefs.Set(WebDriverLog::kPerformanceType, "INFO");
  base::Value::Dict desired_caps;
  desired_caps.Set("goog:loggingPrefs", std::move(logging_prefs));
  ASSERT_EQ("", capabilities.perf_logging_prefs.trace_categories);
  base::Value::Dict perf_logging_prefs;
  perf_logging_prefs.Set("traceCategories", "benchmark,blink.console");
  perf_logging_prefs.Set("bufferUsageReportingInterval", 1234);
  desired_caps.SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                               std::move(perf_logging_prefs));
  Status status = capabilities.Parse(desired_caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("benchmark,blink.console",
            capabilities.perf_logging_prefs.trace_categories);
  ASSERT_EQ(1234,
            capabilities.perf_logging_prefs.buffer_usage_reporting_interval);
}

TEST(ParseCapabilities, PerfLoggingPrefsInvalidInterval) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::Value::Dict logging_prefs;
  logging_prefs.Set(WebDriverLog::kPerformanceType, "INFO");
  base::Value::Dict desired_caps;
  desired_caps.Set("goog:loggingPrefs", std::move(logging_prefs));
  base::Value::Dict perf_logging_prefs;
  // A bufferUsageReportingInterval interval <= 0 will cause DevTools errors.
  perf_logging_prefs.Set("bufferUsageReportingInterval", 0);
  desired_caps.SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                               std::move(perf_logging_prefs));
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsNotDict) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::Value::Dict logging_prefs;
  logging_prefs.Set(WebDriverLog::kPerformanceType, "INFO");
  base::Value::Dict desired_caps;
  desired_caps.Set("goog:loggingPrefs", std::move(logging_prefs));
  desired_caps.SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                               "traceCategories");
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsNoPerfLogLevel) {
  Capabilities capabilities;
  base::Value::Dict desired_caps;
  base::Value::Dict perf_logging_prefs;
  perf_logging_prefs.Set("enableNetwork", true);
  desired_caps.SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                               std::move(perf_logging_prefs));
  // Should fail because perf log must be enabled if perf log prefs specified.
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsPerfLogOff) {
  Capabilities capabilities;
  base::Value::Dict logging_prefs;
  // Disable performance log by setting logging level to OFF.
  logging_prefs.Set(WebDriverLog::kPerformanceType, "OFF");
  base::Value::Dict desired_caps;
  desired_caps.Set("goog:loggingPrefs", std::move(logging_prefs));
  base::Value::Dict perf_logging_prefs;
  perf_logging_prefs.Set("enableNetwork", true);
  desired_caps.SetByDottedPath("goog:chromeOptions.perfLoggingPrefs",
                               std::move(perf_logging_prefs));
  // Should fail because perf log must be enabled if perf log prefs specified.
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, ExcludeSwitches) {
  Capabilities capabilities;
  base::Value::List exclude_switches;
  exclude_switches.Append("switch1");
  exclude_switches.Append("switch2");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.excludeSwitches",
                       base::Value(std::move(exclude_switches)));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.exclude_switches.size());
  const std::set<std::string>& switches = capabilities.exclude_switches;
  ASSERT_TRUE(base::Contains(switches, "switch1"));
  ASSERT_TRUE(base::Contains(switches, "switch2"));
}

TEST(ParseCapabilities, UseRemoteBrowserHostName) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.debuggerAddress", "abc:123");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("abc", capabilities.debugger_address.host());
  ASSERT_EQ(123, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, UseRemoteBrowserIpv4) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.debuggerAddress", "127.0.0.1:456");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("127.0.0.1", capabilities.debugger_address.host());
  ASSERT_EQ(456, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, UseRemoteBrowserIpv6) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.debuggerAddress",
                       "[fe80::f2ef:86ff:fe69:cafe]:789");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("[fe80::f2ef:86ff:fe69:cafe]",
            capabilities.debugger_address.host());
  ASSERT_EQ(789, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, MobileEmulationUserAgent) {
  Capabilities capabilities;
  base::Value::Dict mobile_emulation;
  mobile_emulation.Set("userAgent", "Agent Smith");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation",
                       std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("user-agent"));
  ASSERT_EQ("Agent Smith", capabilities.switches.GetSwitchValue("user-agent"));
}

TEST(ParseCapabilities, MobileEmulationDeviceMetrics) {
  Capabilities capabilities;
  base::Value::Dict mobile_emulation;
  mobile_emulation.SetByDottedPath("deviceMetrics.width", 360);
  mobile_emulation.SetByDottedPath("deviceMetrics.height", 640);
  mobile_emulation.SetByDottedPath("deviceMetrics.pixelRatio", 3.0);
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation",
                       std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(360, capabilities.mobile_device->device_metrics->width);
  ASSERT_EQ(640, capabilities.mobile_device->device_metrics->height);
  ASSERT_EQ(3.0,
            capabilities.mobile_device->device_metrics->device_scale_factor);
}

TEST(ParseCapabilities, MobileEmulationDeviceName) {
  Capabilities capabilities;
  base::Value::Dict mobile_emulation;
  mobile_emulation.Set("deviceName", "Nexus 5");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation",
                       std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("user-agent"));
  ASSERT_TRUE(base::MatchPattern(
      capabilities.switches.GetSwitchValue("user-agent"),
      "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/*.*.*.* Mobile "
      "Safari/537.36"));

  ASSERT_EQ(360, capabilities.mobile_device->device_metrics->width);
  ASSERT_EQ(640, capabilities.mobile_device->device_metrics->height);
  ASSERT_EQ(3.0,
            capabilities.mobile_device->device_metrics->device_scale_factor);
}

TEST(ParseCapabilities, MobileEmulationNotDict) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation", "Google Nexus 5");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationDeviceMetricsNotDict) {
  Capabilities capabilities;
  base::Value::Dict mobile_emulation;
  mobile_emulation.Set("deviceMetrics", 360);
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation",
                       std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationDeviceMetricsNotNumbers) {
  Capabilities capabilities;
  base::Value::Dict mobile_emulation;
  mobile_emulation.SetByDottedPath("deviceMetrics.width", "360");
  mobile_emulation.SetByDottedPath("deviceMetrics.height", "640");
  mobile_emulation.SetByDottedPath("deviceMetrics.pixelRatio", "3.0");
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation",
                       std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationBadDict) {
  Capabilities capabilities;
  base::Value::Dict mobile_emulation;
  mobile_emulation.Set("deviceName", "Google Nexus 5");
  mobile_emulation.SetByDottedPath("deviceMetrics.width", 360);
  mobile_emulation.SetByDottedPath("deviceMetrics.height", 640);
  mobile_emulation.SetByDottedPath("deviceMetrics.pixelRatio", 3.0);
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.mobileEmulation",
                       std::move(mobile_emulation));
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsBool) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("webauthn:virtualAuthenticators", true);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());

  caps.Set("webauthn:virtualAuthenticators", false);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsNotBool) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("webauthn:virtualAuthenticators", "not a bool");
  EXPECT_FALSE(capabilities.Parse(caps).IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsLargeBlobBool) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("webauthn:extension:largeBlob", true);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());

  caps.Set("webauthn:extension:largeBlob", false);
  EXPECT_TRUE(capabilities.Parse(caps).IsOk());
}

TEST(ParseCapabilities, VirtualAuthenticatorsLargeBlobNotBool) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.Set("webauthn:extension:largeBlob", "not a bool");
  EXPECT_FALSE(capabilities.Parse(caps).IsOk());
}
