// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_NEXE_LOAD_MANAGER_H_
#define COMPONENTS_NACL_RENDERER_NEXE_LOAD_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "url/gurl.h"

namespace content {
class PepperPluginInstance;
}

namespace nacl {

class ManifestServiceChannel;
class TrustedPluginChannel;

// NexeLoadManager provides methods for reporting the progress of loading a
// nexe.
class NexeLoadManager {
 public:
  explicit NexeLoadManager(PP_Instance instance);
  ~NexeLoadManager();

  void NexeFileDidOpen(int32_t pp_error,
                       const base::File& file,
                       int32_t http_status,
                       int64_t nexe_bytes_read,
                       const std::string& url,
                       base::TimeDelta time_since_open);
  void ReportLoadSuccess(const std::string& url,
                         uint64_t loaded_bytes,
                         uint64_t total_bytes);
  void ReportLoadError(PP_NaClError error,
                       const std::string& error_message);

  // console_message is a part of the error that is logged to
  // the JavaScript console but is not reported to JavaScript via
  // the lastError property.  This is used to report internal errors which
  // may easily change in new versions of the browser and we don't want apps
  // to come to depend on the details of these errors.
  void ReportLoadError(PP_NaClError error,
                       const std::string& error_message,
                       const std::string& console_message);
  void ReportLoadAbort();
  void NexeDidCrash();

  // TODO(dmichael): Everything below this comment should eventually be made
  // private, when ppb_nacl_private_impl.cc is no longer using them directly.
  // The intent is for this class to only expose functions for reporting a
  // load state transition (e.g., ReportLoadError, ReportProgress,
  // ReportLoadAbort, etc.)
  void set_trusted_plugin_channel(
      std::unique_ptr<TrustedPluginChannel> channel);
  void set_manifest_service_channel(
      std::unique_ptr<ManifestServiceChannel> channel);

  PP_NaClReadyState nacl_ready_state();
  void set_nacl_ready_state(PP_NaClReadyState ready_state);

  void SetReadOnlyProperty(PP_Var key, PP_Var value);
  void SetLastError(const std::string& error);
  void LogToConsole(const std::string& message);

  bool is_installed() const { return is_installed_; }

  int32_t exit_status() const { return exit_status_; }
  void set_exit_status(int32_t exit_status);

  void InitializePlugin(uint32_t argc, const char* argn[], const char* argv[]);

  void ReportStartupOverhead() const;

  int64_t nexe_size() const { return nexe_size_; }

  bool RequestNaClManifest(const std::string& url);
  void ProcessNaClManifest(const std::string& program_url);

  void CloseTrustedPluginChannel();

  // URL resolution support.
  // plugin_base_url is the URL used for resolving relative URLs used in
  // src="...".
  const GURL& plugin_base_url() const { return plugin_base_url_; }

  // manifest_base_url is the URL used for resolving relative URLs mentioned
  // in manifest files.  If the manifest is a data URI, this is an empty string
  const GURL& manifest_base_url() const { return manifest_base_url_; }

  // Returns the manifest URL passed as an argument for this plugin instance.
  std::string GetManifestURLArgument() const;

  // Returns true if the MIME type for this plugin matches the type for PNaCl,
  // false otherwise.
  bool IsPNaCl() const;

  // Returns the time that the work for PNaCl translation began.
  base::Time pnacl_start_time() const { return pnacl_start_time_; }
  void set_pnacl_start_time(base::Time time) {
    pnacl_start_time_ = time;
  }

  const std::string& program_url() const { return program_url_; }

  void set_crash_info_shmem_region(
      base::ReadOnlySharedMemoryRegion shmem_region) {
    crash_info_shmem_region_ = std::move(shmem_region);
  }

  bool nonsfi() const { return nonsfi_; }
  void set_nonsfi(bool nonsfi) { nonsfi_ = nonsfi; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NexeLoadManager);

  void ReportDeadNexe();

  // Copies a crash log to the console, one line at a time.
  void CopyCrashLogToJsConsole(const std::string& crash_log);

  PP_Instance pp_instance_;
  PP_NaClReadyState nacl_ready_state_;
  bool nexe_error_reported_;

  std::string program_url_;

  // A flag indicating if the NaCl executable is being loaded from an installed
  // application.  This flag is used to bucket UMA statistics more precisely to
  // help determine whether nexe loading problems are caused by networking
  // issues.  (Installed applications will be loaded from disk.)
  // Unfortunately, the definition of what it means to be part of an installed
  // application is a little murky - for example an installed application can
  // register a mime handler that loads NaCl executables into an arbitrary web
  // page.  As such, the flag actually means "our best guess, based on the URLs
  // for NaCl resources that we have seen so far".
  bool is_installed_;

  // Time of a successful nexe load.
  base::Time ready_time_;

  // Time of plugin initialization.
  base::Time init_time_;

  // Time of the start of loading a NaCl module.
  base::Time load_start_;

  // The exit status of the plugin process.
  // This will have a value in the range (0x00-0xff) if the exit status is set,
  // or -1 if set_exit_status() has never been called.
  int32_t exit_status_;

  // Size of the downloaded nexe, in bytes.
  int64_t nexe_size_;

  // Non-owning.
  content::PepperPluginInstance* plugin_instance_;

  // The URL for the document corresponding to this plugin instance.
  GURL plugin_base_url_;

  GURL manifest_base_url_;

  // Arguments passed to this plugin instance from the DOM.
  std::map<std::string, std::string> args_;

  // We store mime_type_ outside of args_ explicitly because we change it to be
  // lowercase.
  std::string mime_type_;

  base::Time pnacl_start_time_;

  // A flag that indicates if the plugin is using Non-SFI mode.
  bool nonsfi_;

  base::ReadOnlySharedMemoryRegion crash_info_shmem_region_;

  std::unique_ptr<TrustedPluginChannel> trusted_plugin_channel_;
  std::unique_ptr<ManifestServiceChannel> manifest_service_channel_;
  base::WeakPtrFactory<NexeLoadManager> weak_factory_{this};
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_NEXE_LOAD_MANAGER_H_
