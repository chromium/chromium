// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_CRASH_REPORTER_CLIENT_H_
#define COMPONENTS_CRASH_CORE_APP_CRASH_REPORTER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "build/build_config.h"

#if !BUILDFLAG(IS_WIN)
namespace base {
class FilePath;
}
#endif

namespace crash_reporter {

class CrashReporterClient;

// Setter and getter for the client.  The client should be set early, before any
// crash reporter code is called, and should stay alive throughout the entire
// runtime.
void SetCrashReporterClient(CrashReporterClient* client);

#if defined(CRASH_IMPLEMENTATION)
// The components's embedder API should only be used by the component.
CrashReporterClient* GetCrashReporterClient();
#endif

// Interface that the embedder implements.
class CrashReporterClient {
 public:
  CrashReporterClient();
  virtual ~CrashReporterClient();

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID)
  // Sets the crash reporting client ID, a unique identifier for the client
  // that is sending crash reports. After it is set, it should not be changed.
  // |client_guid| may either be a full GUID or a GUID that was already stripped
  // from its dashes.
  //
  // On macOS, Windows, and Android this is the responsibility of Crashpad, and
  // can not be set directly by the client.
  virtual void SetCrashReporterClientIdFromGUID(const std::string& client_guid);
#endif

#if BUILDFLAG(IS_WIN)
  // Returns true if the pipe name to connect to breakpad should be computed and
  // stored in the process's environment block. By default, returns true for the
  // "browser" process.
  virtual bool ShouldCreatePipeName(const std::wstring& process_type);

  // Returns true if an alternative location to store the minidump files was
  // specified. Returns true if |crash_dir| was set.
  virtual bool GetAlternativeCrashDumpLocation(std::wstring* crash_dir);

  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(const std::wstring& exe_path,
                                        std::wstring* product_name,
                                        std::wstring* version,
                                        std::wstring* special_build,
                                        std::wstring* channel_name);

  // Returns true if a restart dialog should be displayed. In that case,
  // |message| and |title| are set to a message to display in a dialog box with
  // the given title before restarting, and |is_rtl_locale| indicates whether
  // to display the text as RTL.
  virtual bool ShouldShowRestartDialog(std::wstring* title,
                                       std::wstring* message,
                                       bool* is_rtl_locale);

  // Returns true if it is ok to restart the application. Invoked right before
  // restarting after a crash.
  virtual bool AboutToRestart();

  // Returns true if the running binary is a per-user installation.
  virtual bool GetIsPerUserInstall();

  // Returns the result code to return when breakpad failed to respawn a
  // crashed process.
  virtual int GetResultCodeRespawnFailed();

  // Returns the fully-qualified path for a registered out of process exception
  // helper module. The module is optional. Return an empty string to indicate
  // that no module should be registered.
  virtual std::wstring GetWerRuntimeExceptionModule();
#endif

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  // Returns true if larger crash dumps should be dumped.
  virtual bool GetShouldDumpLargerDumps();
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Returns a textual description of the product type and version to include
  // in the crash report. Neither out parameter should be set to NULL.
  // TODO(jperaza): Remove the 2-parameter overload of this method once all
  // Linux-ish breakpad clients have transitioned to crashpad.
  virtual void GetProductNameAndVersion(const char** product_name,
                                        const char** version);
  virtual void GetProductNameAndVersion(std::string* product_name,
                                        std::string* version,
                                        std::string* channel);

  virtual base::FilePath GetReporterLogFilename();

  // Custom crash minidump handler after the minidump is generated.
  // Returns true if the minidump is handled (client); otherwise, return false
  // to fallback to default handler.
  // WARNING: this handler runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  virtual bool HandleCrashDump(const char* crashdump_filename,
                               uint64_t crash_pid);
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set. Windows has to use std::wstring because this code
  // needs to work in chrome_elf, where only kernel32.dll is allowed, and
  // base::FilePath and its dependencies pull in other DLLs.
#if BUILDFLAG(IS_WIN)
  virtual bool GetCrashDumpLocation(std::wstring* crash_dir);
#else
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir);
#endif

  // The location where metrics files should be written. Returns true if
  // |metrics_dir| was set. Windows has to use std::wstring because this code
  // needs to work in chrome_elf, where only kernel32.dll is allowed, and
  // base::FilePath and its dependencies pull in other DLLs.
#if BUILDFLAG(IS_WIN)
  virtual bool GetCrashMetricsLocation(std::wstring* metrics_dir);
#else
  virtual bool GetCrashMetricsLocation(base::FilePath* metrics_dir);
#endif

  // Returns true if running in unattended mode (for automated testing).
  virtual bool IsRunningUnattended();

  // Returns true if the user has given consent to collect stats.
  virtual bool GetCollectStatsConsent();

  // Returns true if the client is currently in the chosen sample that will
  // report stats and crashes. Crashes should only be reported if this function
  // returns true and GetCollectStatsConsent returns true.
  virtual bool GetCollectStatsInSample();

  // Returns true if crash reporting is enforced via management policies. In
  // that case, |breakpad_enabled| is set to the value enforced by policies.
  virtual bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled);

#if BUILDFLAG(IS_ANDROID)
  // Used by WebView to sample crashes without generating the unwanted dumps. If
  // the returned value is less than 100, crash dumping will be sampled to that
  // percentage.
  virtual unsigned int GetCrashDumpPercentage();

  // Returns true if |ptype| was set to a value to override the default `ptype`
  // annotation used for the browser process.
  virtual bool GetBrowserProcessType(std::string* ptype);

  // Returns the descriptor key of the android minidump global descriptor.
  virtual int GetAndroidMinidumpDescriptor();

  // Returns the file descriptor of the pipe used to inform apps of
  // webview renderer crashes.
  virtual int GetAndroidCrashSignalFD();

  // Returns true if breakpad microdumps should be enabled. This orthogonal to
  // the standard minidump uploader (which depends on the user consent).
  virtual bool ShouldEnableBreakpadMicrodumps();

  // Returns true if minudump should be written to android log.
  virtual bool ShouldWriteMinidumpToLog();
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Configures sanitization of crash dumps.
  // |allowed_annotations| is a nullptr terminated array of NUL-terminated
  // strings of allowed annotation names or nullptr if all annotations are
  // allowed. |target_module| is a pointer to a location inside a module to
  // target or nullptr if there is no target module. Crash dumps are not
  // produced when the crashing thread's stack and program counter do not
  // reference the target module. |sanitize_stacks| is true if stacks should be
  // sanitized for possible PII. If they are sanitized, only small integers and
  // pointers to modules and stacks will be preserved.
  virtual void GetSanitizationInformation(
      const char* const** allowed_annotations,
      void** target_module,
      bool* sanitize_stacks);
#endif

  // Returns the URL target for crash report uploads.
  virtual std::string GetUploadUrl();

  // This method should return true to configure a crash reporter capable of
  // monitoring itself for its own crashes to do so, even if self-monitoring
  // would be expensive. "Expensive" self-monitoring dedicates an additional
  // crash handler process to handle the crashes of the initial crash handler
  // process.
  //
  // In some cases, inexpensive self-monitoring may also be available. When it
  // is, it may be used when this method returns false. If only expensive
  // self-monitoring is available, returning false from this function will
  // prevent the crash handler process from being monitored for crashes at all.
  //
  // The default implementation returns false.
  virtual bool ShouldMonitorCrashHandlerExpensively();

  // Returns true if breakpad should run in the given process type.
  virtual bool EnableBreakpadForProcess(const std::string& process_type);
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_CRASH_REPORTER_CLIENT_H_
