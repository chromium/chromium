// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_COORDINATOR_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_COORDINATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/nacl/renderer/plugin/nacl_subprocess.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "components/nacl/renderer/plugin/pnacl_resources.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/utility/completion_callback_factory.h"

struct PP_PNaClOptions;

namespace plugin {

class Plugin;
class PnaclCoordinator;
class PnaclTranslateThread;

// A class invoked by Plugin to handle PNaCl client-side translation.
// Usage:
// (1) Invoke the factory method, e.g.,
//     PnaclCoordinator* coord = BitcodeToNative(plugin,
//                                               "http://foo.com/my.pexe",
//                                               pnacl_options,
//                                               translate_notify_callback);
// (2) translate_notify_callback gets invoked when translation is complete.
//     If the translation was successful, the pp_error argument is PP_OK.
//     Other values indicate errors.
// (3) After finish_callback runs, get the file descriptor of the translated
//     nexe, e.g.,
//     fd = coord->TakeTranslatedFileHandle();
// (4) Load the nexe from "fd".
// (5) delete coord.
//
// Translation proceeds in two steps:
// (1) llc translates the bitcode in pexe_url_ to an object in obj_file_.
// (2) ld links the object code in obj_file_ and produces a nexe in nexe_file_.
class PnaclCoordinator {
 public:
  PnaclCoordinator(const PnaclCoordinator&) = delete;
  PnaclCoordinator& operator=(const PnaclCoordinator&) = delete;

  virtual ~PnaclCoordinator();

  // The factory method for translations.
  static PnaclCoordinator* BitcodeToNative(
      Plugin* plugin,
      const std::string& pexe_url,
      const PP_PNaClOptions& pnacl_options,
      const pp::CompletionCallback& translate_notify_callback);

  // Call this to take ownership of the FD of the translated nexe after
  // BitcodeToNative has completed (and the finish_callback called).
  PP_FileHandle TakeTranslatedFileHandle();

  // Return a callback that should be notified when |bytes_compiled| bytes
  // have been compiled.
  pp::CompletionCallback GetCompileProgressCallback(int64_t bytes_compiled);

  // Return true if we should delay the progress event reporting.
  // This delay approximates:
  // - the size of the buffer of bytes sent but not-yet-compiled by LLC.
  // - the linking time.
  bool ShouldDelayProgressEvent() {
    const uint32_t kProgressEventSlopPct = 5;
    return ((expected_pexe_size_ - pexe_bytes_compiled_) * 100 /
            expected_pexe_size_) < kProgressEventSlopPct;
  }


  void BitcodeStreamCacheHit(PP_FileHandle handle);
  void BitcodeStreamCacheMiss(int64_t expected_pexe_size,
                              PP_FileHandle handle);

  // Invoked when a pexe data chunk arrives (when using streaming translation)
  void BitcodeStreamGotData(const void* data, int32_t length);

  // Invoked when the pexe download finishes (using streaming translation)
  void BitcodeStreamDidFinish(int32_t pp_error);

 private:
  // BitcodeToNative is the factory method for PnaclCoordinators.
  // Therefore the constructor is private.
  PnaclCoordinator(Plugin* plugin,
                   const std::string& pexe_url,
                   const PP_PNaClOptions& pnacl_options,
                   const pp::CompletionCallback& translate_notify_callback);

  // Invoke to issue a GET request for bitcode.
  void OpenBitcodeStream();

  // Invoked when a pexe data chunk is compiled.
  void BitcodeGotCompiled(int32_t pp_error, int64_t bytes_compiled);
  // Once llc and ld nexes have been loaded and the two temporary files have
  // been created, this starts the translation.  Translation starts two
  // subprocesses, one for llc and one for ld.
  void LoadCompiler();
  void RunCompile(int32_t pp_error, base::TimeTicks compile_load_start_time);
  void LoadLinker(int32_t pp_error);
  void RunLink(int32_t pp_error, base::TimeTicks ld_load_start_time);

  // Invoked when translation is finished.
  void TranslateFinished(int32_t pp_error);

  // Invoked when the read descriptor for nexe_file_ is created.
  void NexeReadDidOpen();

  // Bring control back to the plugin by invoking the
  // |translate_notify_callback_|.  This does not set the ErrorInfo report,
  // it is assumed that it was already set.
  void ExitWithError();
  // Run |translate_notify_callback_| with an error condition that is not
  // PPAPI specific.  Also set ErrorInfo report.
  void ReportNonPpapiError(PP_NaClError err, const std::string& message);

  // Keeps track of the pp_error upon entry to TranslateFinished,
  // for inspection after cleanup.
  int32_t translate_finish_error_;

  // The plugin owning the nexe for which we are doing translation.
  raw_ptr<Plugin> plugin_;

  pp::CompletionCallback translate_notify_callback_;
  // Set to true when the translation (if applicable) is finished and the nexe
  // file is loaded, (or when there was an error), and the browser has been
  // notified via ReportTranslationFinished. If it is not set before
  // plugin/coordinator destruction, the destructor will call
  // ReportTranslationFinished.
  bool translation_finished_reported_;
  // Threadsafety is required to support file lookups.
  pp::CompletionCallbackFactory<PnaclCoordinator,
                                pp::ThreadSafeThreadTraits> callback_factory_;

  // An auxiliary class that manages downloaded resources (llc and ld nexes).
  std::unique_ptr<PnaclResources> resources_;
  NaClSubprocess compiler_subprocess_;
  NaClSubprocess ld_subprocess_;

  // The URL for the pexe file.
  std::string pexe_url_;
  // Options for translation.
  PP_PNaClOptions pnacl_options_;
  // Architecture-specific attributes used for translation. These are
  // supplied by Chrome, not the developer, and are therefore different
  // from PNaCl options.
  std::string architecture_attributes_;

  // Object file, produced by the translator and consumed by the linker.
  std::vector<base::File> obj_files_;
  // Number of split modules for llc.
  int split_module_count_;
  // Number of threads for llc / subzero.
  int num_threads_;

  // Translated nexe file, produced by the linker.
  base::File temp_nexe_file_;

  // Used to report information when errors (PPAPI or otherwise) are reported.
  ErrorInfo error_info_;

  // True if an error was already reported, and translate_notify_callback_
  // was already run/consumed.
  bool error_already_reported_;

  // State for timing and size information for UMA stats.
  int64_t pexe_size_;  // Count as we stream -- will converge to pexe size.
  int64_t pexe_bytes_compiled_;  // Count as we compile.
  int64_t expected_pexe_size_;   // Expected download total (-1 if unknown).

  // The helper thread used to do translations via SRPC.
  // It accesses fields of PnaclCoordinator so it must have a
  // shorter lifetime.
  std::unique_ptr<PnaclTranslateThread> translate_thread_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_COORDINATOR_H_
