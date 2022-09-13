// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PPB_NACL_PRIVATE_H_
#define COMPONENTS_NACL_RENDERER_PPB_NACL_PRIVATE_H_

#include <memory>

#include "base/process/process_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppp_pexe_stream_handler.h"

// This file defines an interface that is used by the code in
// components/nacl/renderer/plugin/.  It is defined as a PPAPI PPB_*
// interface for historical reasons: The code in plugin/ used to live
// outside the Chromium repo.
//
// In the future, we may integrate the code in nacl/renderer/plugin/ more
// closely with nacl/renderer/.  This could include having plugin/ call the
// functions below directly, rather than providing these functions via the
// PPB_NaCl_Private interface struct.

/**
 * @addtogroup Enums
 * @{
 */
/** NaCl-specific errors that should be reported to the user.
 *  These error codes are reported via UMA so, if you edit them:
 *   1) make sure you understand UMA first.
 *   2) update src/tools/metrics/histograms/histograms.xml
 *  Values are explicitly specified to make sure they don't shift around when
 *  edited, and also to make reading about:histograms easier.
 */
typedef enum {
  PP_NACL_ERROR_LOAD_SUCCESS = 0,
  PP_NACL_ERROR_LOAD_ABORTED = 1,
  PP_NACL_ERROR_UNKNOWN = 2,
  PP_NACL_ERROR_MANIFEST_RESOLVE_URL = 3,
  PP_NACL_ERROR_MANIFEST_LOAD_URL = 4,
  PP_NACL_ERROR_MANIFEST_STAT = 5,
  PP_NACL_ERROR_MANIFEST_TOO_LARGE = 6,
  PP_NACL_ERROR_MANIFEST_OPEN = 7,
  PP_NACL_ERROR_MANIFEST_MEMORY_ALLOC = 8,
  PP_NACL_ERROR_MANIFEST_READ = 9,
  PP_NACL_ERROR_MANIFEST_PARSING = 10,
  PP_NACL_ERROR_MANIFEST_SCHEMA_VALIDATE = 11,
  PP_NACL_ERROR_MANIFEST_GET_NEXE_URL = 12,
  PP_NACL_ERROR_NEXE_LOAD_URL = 13,
  PP_NACL_ERROR_NEXE_ORIGIN_PROTOCOL = 14,
  PP_NACL_ERROR_NEXE_FH_DUP = 15,
  PP_NACL_ERROR_NEXE_STAT = 16,
  PP_NACL_ERROR_ELF_CHECK_IO = 17,
  PP_NACL_ERROR_ELF_CHECK_FAIL = 18,
  PP_NACL_ERROR_SEL_LDR_INIT = 19,
  PP_NACL_ERROR_SEL_LDR_CREATE_LAUNCHER = 20,
  PP_NACL_ERROR_SEL_LDR_FD = 21,
  PP_NACL_ERROR_SEL_LDR_LAUNCH = 22,
  /* Deprecated, safe to reuse the value because it's never logged in UMA.
   */
  PP_NACL_ERROR_SEL_LDR_SEND_NEXE = 24,
  PP_NACL_ERROR_SEL_LDR_HANDLE_PASSING = 25,
  PP_NACL_ERROR_SEL_LDR_START_MODULE = 26,
  PP_NACL_ERROR_SEL_LDR_START_STATUS = 27,
  PP_NACL_ERROR_SRPC_CONNECTION_FAIL = 28,
  PP_NACL_ERROR_START_PROXY_CHECK_PPP = 29,
  PP_NACL_ERROR_START_PROXY_ALLOC = 30,
  PP_NACL_ERROR_START_PROXY_MODULE = 31,
  PP_NACL_ERROR_START_PROXY_INSTANCE = 32,
  PP_NACL_ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL = 33,
  PP_NACL_ERROR_SEL_LDR_COMMUNICATION_REV_SETUP = 34,
  PP_NACL_ERROR_SEL_LDR_COMMUNICATION_WRAPPER = 35,
  PP_NACL_ERROR_SEL_LDR_COMMUNICATION_REV_SERVICE = 36,
  PP_NACL_ERROR_START_PROXY_CRASH = 37,
  PP_NACL_ERROR_MANIFEST_PROGRAM_MISSING_ARCH = 38,
  PP_NACL_ERROR_PNACL_CACHE_OPEN_INPROGRESS = 39,
  PP_NACL_ERROR_PNACL_CACHE_OPEN_NOACCESS = 40,
  PP_NACL_ERROR_PNACL_CACHE_OPEN_NOQUOTA = 41,
  PP_NACL_ERROR_PNACL_CACHE_OPEN_NOSPACE = 42,
  PP_NACL_ERROR_PNACL_CACHE_OPEN_OTHER = 43,
  PP_NACL_ERROR_PNACL_CACHE_DIRECTORY_CREATE = 44,
  PP_NACL_ERROR_PNACL_CACHE_FILEOPEN_NOACCESS = 45,
  PP_NACL_ERROR_PNACL_CACHE_FILEOPEN_NOQUOTA = 46,
  PP_NACL_ERROR_PNACL_CACHE_FILEOPEN_NOSPACE = 47,
  PP_NACL_ERROR_PNACL_CACHE_FILEOPEN_NOTAFILE = 48,
  PP_NACL_ERROR_PNACL_CACHE_FILEOPEN_OTHER = 49,
  PP_NACL_ERROR_PNACL_CACHE_FETCH_NOACCESS = 50,
  PP_NACL_ERROR_PNACL_CACHE_FETCH_NOTFOUND = 51,
  PP_NACL_ERROR_PNACL_CACHE_FETCH_OTHER = 52,
  PP_NACL_ERROR_PNACL_CACHE_FINALIZE_COPY_NOQUOTA = 53,
  PP_NACL_ERROR_PNACL_CACHE_FINALIZE_COPY_NOSPACE = 54,
  PP_NACL_ERROR_PNACL_CACHE_FINALIZE_COPY_OTHER = 55,
  PP_NACL_ERROR_PNACL_CACHE_FINALIZE_RENAME_NOACCESS = 56,
  PP_NACL_ERROR_PNACL_CACHE_FINALIZE_RENAME_OTHER = 57,
  PP_NACL_ERROR_PNACL_RESOURCE_FETCH = 58,
  PP_NACL_ERROR_PNACL_PEXE_FETCH_ABORTED = 59,
  PP_NACL_ERROR_PNACL_PEXE_FETCH_NOACCESS = 60,
  PP_NACL_ERROR_PNACL_PEXE_FETCH_OTHER = 61,
  PP_NACL_ERROR_PNACL_THREAD_CREATE = 62,
  PP_NACL_ERROR_PNACL_LLC_SETUP = 63,
  PP_NACL_ERROR_PNACL_LD_SETUP = 64,
  PP_NACL_ERROR_PNACL_LLC_INTERNAL = 65,
  PP_NACL_ERROR_PNACL_LD_INTERNAL = 66,
  PP_NACL_ERROR_PNACL_CREATE_TEMP = 67,
  /* This entry is no longer used, but should not be removed, because UMA
   */
  PP_NACL_ERROR_PNACL_NOT_ENABLED = 68,
  PP_NACL_ERROR_MANIFEST_NOACCESS_URL = 69,
  PP_NACL_ERROR_NEXE_NOACCESS_URL = 70,
  PP_NACL_ERROR_PNACL_CRASH_THROTTLED = 71,
  /* If you add a code, read the enum comment above on how to update
   */
  PP_NACL_ERROR_MAX
} PP_NaClError;

/** Event types that NaCl may use when reporting load progress or errors. */
typedef enum {
  PP_NACL_EVENT_LOADSTART,
  PP_NACL_EVENT_PROGRESS,
  PP_NACL_EVENT_ERROR,
  PP_NACL_EVENT_ABORT,
  PP_NACL_EVENT_LOAD,
  PP_NACL_EVENT_LOADEND,
  PP_NACL_EVENT_CRASH
} PP_NaClEventType;

typedef enum {
  PP_SCHEME_CHROME_EXTENSION,
  PP_SCHEME_DATA,
  PP_SCHEME_OTHER
} PP_UrlSchemeType;

typedef enum {
  /* The trusted plugin begins in this ready state. */
  PP_NACL_READY_STATE_UNSENT = 0,
  /* The manifest file has been requested, but not yet received. */
  PP_NACL_READY_STATE_OPENED = 1,
  /* The manifest file has been received and the nexe successfully requested. */
  PP_NACL_READY_STATE_LOADING = 3,
  /* The nexe has been loaded and the proxy started, so it is ready for
   */
  PP_NACL_READY_STATE_DONE = 4
} PP_NaClReadyState;

/** Types of untrusted NaCl processes. Mirrors NaClAppProcessType from
 *  components/nacl/common/nacl_types.h.
 */
typedef enum {
  PP_UNKNOWN_NACL_PROCESS_TYPE,
  PP_NATIVE_NACL_PROCESS_TYPE,
  PP_PNACL_PROCESS_TYPE,
  PP_PNACL_TRANSLATOR_PROCESS_TYPE,
  PP_NUM_NACL_PROCESS_TYPES
} PP_NaClAppProcessType;
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_PNaClOptions {
  PP_Bool translate;
  PP_Bool is_debug;
  PP_Bool use_subzero;
  int32_t opt_level;
};
/**
 * @}
 */

/**
 * @addtogroup Typedefs
 * @{
 */
/* Callback invoked upon completion of PPP_ManifestService::OpenResource(). */
typedef void (*PP_OpenResourceCompletionCallback)(void* user_data,
                                                  PP_FileHandle file_handle);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_NaClFileInfo {
  PP_FileHandle handle;
  /* See NaClFileToken comment in nacl_process_host.h */
  uint64_t token_lo;
  uint64_t token_hi;
};
/**
 * @}
 */

namespace nacl {

// This is a set of interfaces used by the code in
// components/nacl/renderer/plugin/, implemented by
// components/nacl/renderer/.
//
// There is not really a good name for this set of interfaces because the
// grouping exists only for historical reasons.  It used to be a PPAPI
// PPB_* interface (PPB_NaCl_Private) because the code in plugin/ used to
// live outside the Chromium repo and used to be built as a separate
// DSO/DLL.  Since that's no longer the case, there is now no strong
// distinction between renderer/ and renderer/plugin/.
class PPBNaClPrivate {
 public:
  /* Launches NaCl's sel_ldr process.  Returns PP_EXTERNAL_PLUGIN_OK on success.
   * Returns PP_EXTERNAL_PLUGIN_FAILED on failure.
   * The |nexe_file_info| is the file handle for the main nexe file, which
   * should be initially loaded.
   * LaunchSelLdr takes the ownership of the file handle.
   * |translator_channel| is filled out when launching PNaCl translator
   * processes.
   */
  static void LaunchSelLdr(
      PP_Instance instance,
      PP_Bool main_service_runtime,
      const char* alleged_url,
      const struct PP_NaClFileInfo* nexe_file_info,
      PP_NaClAppProcessType process_type,
      std::unique_ptr<IPC::SyncChannel>* translator_channel,
      struct PP_CompletionCallback callback);
  /* Returns a read-only (but executable) file descriptor / file info for
   * a url for pnacl translator tools. Returns an invalid handle on failure.
   */
  static void GetReadExecPnaclFd(const char* url,
                                 struct PP_NaClFileInfo* out_file_info);
  /* This creates a temporary file that will be deleted by the time
   * the last handle is closed (or earlier on POSIX systems), and
   * returns a posix handle to that temporary file.
   */
  static PP_FileHandle CreateTemporaryFile(PP_Instance instance);
  /* Return the number of processors in the system as reported by the OS */
  static int32_t GetNumberOfProcessors();
  /* Report to the browser that translation of the pexe for |instance|
   * has finished, or aborted with an error. If |success| is true, the
   * browser may then store the translation in the cache. The renderer
   * must first have called StreamPexe for the same instance. (The browser is
   * not guaranteed to store the nexe even if |success| is true; if there is
   * an error on the browser side, or the file is too big for the cache, or
   * the browser is in incognito mode, no notification will be delivered to
   * the plugin.)
   */
  static void ReportTranslationFinished(PP_Instance instance,
                                        PP_Bool success,
                                        int32_t opt_level,
                                        PP_Bool use_subzero,
                                        int64_t nexe_size,
                                        int64_t pexe_size,
                                        int64_t compile_time_us);
  /* Dispatch a progress event on the DOM element where the given instance is
   * embedded.
   */
  static void DispatchEvent(PP_Instance instance,
                            PP_NaClEventType event_type,
                            const char* resource_url,
                            PP_Bool length_is_computable,
                            uint64_t loaded_bytes,
                            uint64_t total_bytes);
  /* Report an error that occured while attempting to load a nexe. */
  static void ReportLoadError(PP_Instance instance,
                              PP_NaClError error,
                              const char* error_message);
  /* Performs internal setup when an instance is created. */
  static void InstanceCreated(PP_Instance instance);
  /* Performs internal cleanup when an instance is destroyed. */
  static void InstanceDestroyed(PP_Instance instance);
  /* Terminates the NaCl loader process associated with the instance */
  static void TerminateNaClLoader(PP_Instance instance);
  /* Returns the kind of SFI sandbox implemented by NaCl on this
   * platform.
   */
  static const char* GetSandboxArch(void);
  /* Initializes internal state for a NaCl plugin. */
  static void InitializePlugin(PP_Instance instance,
                               uint32_t argc,
                               const char* argn[],
                               const char* argv[]);
  /* Requests the NaCl manifest specified in the plugin arguments. */
  static void RequestNaClManifest(PP_Instance instance,
                                  struct PP_CompletionCallback callback);
  static struct PP_Var GetManifestBaseURL(PP_Instance instance);
  /* Processes the NaCl manifest once it's been retrieved.
   * TODO(teravest): Move the rest of the supporting logic out of the trusted
   * plugin.
   */
  static void ProcessNaClManifest(PP_Instance instance,
                                  const char* program_url);
  static PP_Bool GetManifestProgramURL(PP_Instance instance,
                                       struct PP_Var* full_url,
                                       struct PP_PNaClOptions* pnacl_options);
  /* Returns the filenames for the llc and ld tools. */
  static PP_Bool GetPnaclResourceInfo(PP_Instance instance,
                                      struct PP_Var* llc_tool_name,
                                      struct PP_Var* ld_tool_name,
                                      struct PP_Var* subzero_tool_name);
  /* PP_Var string of attributes describing the CPU features supported
   * by the current architecture. The string is a comma-delimited list
   * of attributes supported by LLVM in its -mattr= option:
   *   http://llvm.org/docs/CommandGuide/llc.html#cmdoption-mattr */
  static struct PP_Var GetCpuFeatureAttrs(void);
  /* Downloads the .nexe file at the given URL to a file, and sets |file_info|
   * to information for a handle to a file containing its contents.
   * If metadata for identity-based validation caching is available
   * then it sets token information in |file_info| (otherwise left untouched).
   */
  static void DownloadNexe(PP_Instance instance,
                           const char* url,
                           struct PP_NaClFileInfo* file_info,
                           struct PP_CompletionCallback callback);
  /* Logs time taken by an operation to UMA histograms.
   * This function is safe to call on any thread.
   */
  static void LogTranslateTime(const char* histogram_name, int64_t time_us);
  /* Logs amount of pexe bytes compiled when download is complete. */
  static void LogBytesCompiledVsDownloaded(PP_Bool use_subzero,
                                           int64_t pexe_bytes_compiled,
                                           int64_t pexe_bytes_downloaded);
  /* Sets the start time for PNaCl downloading and translation to the current
   * time.
   */
  static void SetPNaClStartTime(PP_Instance instance);
  /* Downloads and streams a pexe file for PNaCl translation.
   * Fetches the content at |pexe_url| for the given instance and opt_level.
   * If a translated cached nexe is already available,
   * the |stream_handler|'s |DidCacheHit| is called. Otherwise, |DidCacheMiss|
   * is called and |DidStreamData| is called repeatedly with blocks of data
   * as they are received. |DidFinishStream| is called after all
   * data has been received and dispatched to |DidStreamData|.
   */
  static void StreamPexe(PP_Instance instance,
                         const char* pexe_url,
                         int32_t opt_level,
                         PP_Bool use_subzero,
                         const struct PPP_PexeStreamHandler_1_0* stream_handler,
                         void* stream_handler_user_data);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_PPB_NACL_PRIVATE_H_
