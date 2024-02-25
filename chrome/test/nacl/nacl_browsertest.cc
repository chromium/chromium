// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "build/build_config.h"

#define TELEMETRY 1

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

namespace {

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_SimpleLoad, {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, ExitStatus0, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit0&expected_exit=0"));
})

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_ExitStatus254, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit254&expected_exit=254"));
})

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_ExitStatusNeg2, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exitneg2&expected_exit=254"));
})

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_PPAPICore, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppb_core.html"));
})

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_PPAPIPPBInstance, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppb_instance.html"));
})

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_PPAPIPPPInstance, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppp_instance.html"));
})

// TODO(https://crbug.com/1059468): Flaky.
NACL_BROWSER_TEST_F(NaClBrowserTest, DISABLED_ProgressEvents, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_progress_events.html"));
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, Bad) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_bad.html"));
}

// partially_invalid.c does not have an ARM version of its asm.
#if !defined(__arm__)
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, BadNative) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_bad_native.html"));
}
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, CrashViaCheckFailure, {
  RunNaClIntegrationTest(
      FILE_PATH_LITERAL("ppapi_crash_via_check_failure.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, CrashViaExitCall, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_crash_via_exit_call.html"));
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestIrt, CrashViaLibcxxAssertFailure) {
  RunNaClIntegrationTest(
      FILE_PATH_LITERAL("ppapi_crash_via_libcxx_assert_failure.html"));
}

NACL_BROWSER_TEST_F(NaClBrowserTest, CrashInCallback, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_crash_in_callback.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, CrashOffMainThread, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_crash_off_main_thread.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, CrashPPAPIOffMainThread, {
  RunNaClIntegrationTest(
      FILE_PATH_LITERAL("ppapi_crash_ppapi_off_main_thread.html"));
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, IrtManifestFile) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_manifest_file_test.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, IrtException) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_exception_test.html"));
}

// Some versions of Visual Studio does not like preprocessor
// conditionals inside the argument of a macro, so we put the
// conditionals on a helper function.  We are already in an anonymous
// namespace, so the name of the helper is not visible in external
// scope.
#if BUILDFLAG(IS_POSIX)
base::FilePath::StringType NumberOfCoresAsFilePathString() {
  char string_rep[23];
  long nprocessors = sysconf(_SC_NPROCESSORS_ONLN);
#if TELEMETRY
  fprintf(stderr, "browser says nprocessors = %ld\n", nprocessors);
  // crbug.com/597899
  fflush(stderr);
#endif
  snprintf(string_rep, sizeof string_rep, "%ld", nprocessors);
  return string_rep;
}
#endif

#if TELEMETRY
static void PathTelemetry(base::FilePath::StringType const &path) {
  fprintf(stderr, "path = %s\n", path.c_str());
  // crbug.com/597899
  fflush(stderr);
}
#else
static void PathTelemetry(base::FilePath::StringType const &path) {
  (void) path;
}
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, SysconfNprocessorsOnln, {
  base::FilePath::StringType path =
      FILE_PATH_LITERAL("sysconf_nprocessors_onln_test.html?cpu_count=");
  path = path + NumberOfCoresAsFilePathString();
  PathTelemetry(path);
  RunNaClIntegrationTest(path);
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, CrossOriginCors) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/cors.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, CrossOriginFail) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/fail.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, SameOriginCookie) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/same_origin_cookie.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, CorsNoCookie) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/cors_no_cookie.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, RelativeManifest) {
  RunLoadTest(FILE_PATH_LITERAL("manifest/relative_manifest.html"));
}

// Test with the NaCl debug flag turned on.
class NaClBrowserTestPnaclDebug : public NaClBrowserTestPnacl {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NaClBrowserTestPnacl::SetUpCommandLine(command_line);
    // Turn on debugging to influence the PNaCl URL loaded
    command_line->AppendSwitch(switches::kEnableNaClDebug);
  }

  void StartTestScript(base::Process* test_process,
                       int debug_stub_port) {
    // We call a python script that speaks to the debug stub, and
    // lets the app continue, so that the load progress event completes.
    base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
    base::FilePath script;
    base::PathService::Get(chrome::DIR_TEST_DATA, &script);
    script = script.AppendASCII("nacl/debug_stub_browser_tests.py");
    cmd.AppendArgPath(script);
    cmd.AppendArg(base::NumberToString(debug_stub_port));
    cmd.AppendArg("continue");
    LOG(INFO) << cmd.GetCommandLineString();
    *test_process = base::LaunchProcess(cmd, base::LaunchOptions());
  }

  void RunWithTestDebugger(const base::FilePath::StringType& test_url) {
    base::Process test_script;
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    nacl::NaClBrowser::SetGdbDebugStubPortListenerForTest(
        base::BindRepeating(&NaClBrowserTestPnaclDebug::StartTestScript,
                            base::Unretained(this), &test_script));
    // Turn on debug stub logging.
    env->SetVar("NACLVERBOSITY", "1");
    RunLoadTest(test_url);
    env->UnSetVar("NACLVERBOSITY");
    nacl::NaClBrowser::ClearGdbDebugStubPortListenerForTest();
    int exit_code;
    LOG(INFO) << "Waiting for script to exit (which waits for embed to die).";
    test_script.WaitForExit(&exit_code);
    EXPECT_EQ(0, exit_code);
  }
};

// Test with the NaCl debug flag turned on, but mask off every URL
// so that nothing is actually debugged.
class NaClBrowserTestPnaclDebugMasked : public NaClBrowserTestPnaclDebug {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NaClBrowserTestPnaclDebug::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kNaClDebugMask,
                                    "!<all_urls>");
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDebug, PnaclDebugURLFlagAndURL) {
  RunWithTestDebugger(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_has_debug.nmf"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDebug, PnaclDebugURLFlagNoURL) {
  RunWithTestDebugger(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_no_debug.nmf"));
}

// TODO(https://crbug.com/1059468): Flaky.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl, DISABLED_PnaclDebugURLFlagOff) {
  RunLoadTest(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_has_debug_flag_off.nmf"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDebugMasked,
                       MAYBE_PNACL(PnaclDebugURLFlagMaskedOff)) {
  // If the mask excludes debugging, it's as if the flag was off.
  RunLoadTest(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_has_debug_flag_off.nmf"));
}

// NaClBrowserTestPnacl.PnaclErrorHandling is flaky on Win, Mac, and Linux.
// http://crbug.com/704980, http://crbug.com/870309
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PnaclErrorHandling DISABLED_PnaclErrorHandling
#else
#define MAYBE_PnaclErrorHandling PnaclErrorHandling
#endif

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PnaclErrorHandling) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pnacl_error_handling.html"));
}

// Test Subzero. Subzero is triggered by the O0 option so reuse
// test harnesses that use "optlevel": 0.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclSubzero, MAYBE_PnaclErrorHandling) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pnacl_error_handling.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl, MAYBE_PNACL(PnaclNMFOptionsO0)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_0"));
}

// Test Subzero. Subzero is triggered by the O0 option so reuse
// test harnesses that use "optlevel": 0.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclSubzero,
                       MAYBE_PNACL(PnaclNMFOptionsO0)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_0"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclNMFOptionsO2)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_2"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclNMFOptionsOlarge)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_large"));
}

// TODO(https://crbug.com/1059468): Flaky.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       DISABLED_PnaclDyncodeSyscallDisabled) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pnacl_dyncode_syscall_disabled.html"));
}

// TODO(https://crbug.com/1059468): Flaky.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       DISABLED_PnaclExceptionHandlingDisabled) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pnacl_hw_eh_disabled.html"));
}

// TODO(ncbray) support glibc and PNaCl
// flaky: crbug.com/375894
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibExtension, DISABLED_MimeHandler) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "ppapi_extension_mime_handler.html"));
}

}  // namespace
