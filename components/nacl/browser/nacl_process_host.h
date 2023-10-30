// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_PROCESS_HOST_H_
#define COMPONENTS_NACL_BROWSER_NACL_PROCESS_HOST_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "components/nacl/common/nacl_types.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "ipc/ipc_channel_handle.h"
#include "net/socket/socket_descriptor.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "url/gurl.h"

namespace content {
class BrowserChildProcessHost;
class BrowserPpapiHost;
}

namespace IPC {
class ChannelProxy;
}

namespace nacl {

// NaClFileToken is a single-use nonce that the NaCl loader process can use
// to query the browser process for trusted information about a file.  This
// helps establish that the file is known by the browser to be immutable
// and suitable for file-identity-based validation caching.  lo == 0 && hi
// == 0 indicates the token is invalid and no additional information is
// available.
struct NaClFileToken {
  uint64_t lo;
  uint64_t hi;
};

class NaClHostMessageFilter;
void* AllocateAddressSpaceASLR(base::ProcessHandle process, size_t size);

// Represents the browser side of the browser <--> NaCl communication
// channel. There will be one NaClProcessHost per NaCl process
// The browser is responsible for starting the NaCl process
// when requested by the renderer.
// After that, most of the communication is directly between NaCl plugin
// running in the renderer and NaCl processes.
class NaClProcessHost : public content::BrowserChildProcessHostDelegate {
 public:
  // manifest_url: the URL of the manifest of the Native Client plugin being
  // executed.
  // nexe_file: A file that corresponds to the nexe module to be loaded.
  // nexe_token: A cache validation token for nexe_file.
  // prefetched_resource_files_info: An array of resource files prefetched.
  // permissions: PPAPI permissions, to control access to private APIs.
  // permission_bits: controls which interfaces the NaCl plugin can use.
  // off_the_record: was the process launched from an incognito renderer?
  // process_type: the type of NaCl process.
  // profile_directory: is the path of current profile directory.
  NaClProcessHost(
      const GURL& manifest_url,
      base::File nexe_file,
      const NaClFileToken& nexe_token,
      const std::vector<NaClResourcePrefetchResult>& prefetched_resource_files,
      ppapi::PpapiPermissions permissions,
      uint32_t permission_bits,
      bool off_the_record,
      NaClAppProcessType process_type,
      const base::FilePath& profile_directory);

  NaClProcessHost(const NaClProcessHost&) = delete;
  NaClProcessHost& operator=(const NaClProcessHost&) = delete;

  ~NaClProcessHost() override;

  void OnProcessCrashed(int exit_status) override;

  // Do any minimal work that must be done at browser startup.
  static void EarlyStartup();

  // Specifies throttling time in milliseconds for PpapiHostMsg_Keepalive IPCs.
  static void SetPpapiKeepAliveThrottleForTesting(unsigned milliseconds);

  // Initialize the new NaCl process. Result is returned by sending ipc
  // message reply_msg.
  void Launch(NaClHostMessageFilter* nacl_host_message_filter,
              IPC::Message* reply_msg,
              const base::FilePath& manifest_path);

  void OnChannelConnected(int32_t peer_pid) override;

  bool Send(IPC::Message* msg);

  content::BrowserChildProcessHost* process() { return process_.get(); }
  content::BrowserPpapiHost* browser_ppapi_host() { return ppapi_host_.get(); }

 private:
  void LaunchNaClGdb();

  // Mark the process as using a particular GDB debug stub port and notify
  // listeners (if the port is not kGdbDebugStubPortUnknown).
  void SetDebugStubPort(int port);

#if BUILDFLAG(IS_POSIX)
  // Create bound TCP socket in the browser process so that the NaCl GDB debug
  // stub can use it to accept incoming connections even when the Chrome sandbox
  // is enabled.
  net::SocketDescriptor GetDebugStubSocketHandle();
#endif

  bool LaunchSelLdr();

  // BrowserChildProcessHostDelegate implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnProcessLaunched() override;

  void OnResourcesReady();

  // Sends the reply message to the renderer who is waiting for the plugin
  // to load. Returns true on success.
  void ReplyToRenderer(
      mojo::ScopedMessagePipeHandle ppapi_channel_handle,
      mojo::ScopedMessagePipeHandle trusted_channel_handle,
      mojo::ScopedMessagePipeHandle manifest_service_channel_handle,
      base::ReadOnlySharedMemoryRegion crash_info_shmem_region);

  // Sends the reply with error message to the renderer.
  void SendErrorToRenderer(const std::string& error_message);

  // Sends the reply message to the renderer. Either result or
  // error message must be empty.
  void SendMessageToRenderer(const NaClLaunchResult& result,
                             const std::string& error_message);

  // Sends the message to the NaCl process to load the plugin. Returns true
  // on success.
  bool StartNaClExecution();

  void StartNaClFileResolved(
      NaClStartParams params,
      const base::FilePath& file_path,
      base::File nexe_file);

  // Starts browser PPAPI proxy. Returns true on success.
  bool StartPPAPIProxy(mojo::ScopedMessagePipeHandle channel_handle);

  // Does post-process-launching tasks for starting the NaCl process once
  // we have a connection.
  //
  // Returns false on failure.
  bool StartWithLaunchedProcess();

  // Message handlers for validation caching.
  void OnQueryKnownToValidate(const std::string& signature, bool* result);
  void OnSetKnownToValidate(const std::string& signature);
  void OnResolveFileToken(uint64_t file_token_lo, uint64_t file_token_hi);
  void FileResolved(uint64_t file_token_lo,
                    uint64_t file_token_hi,
                    const base::FilePath& file_path,
                    base::File file);

  // Called when the PPAPI IPC channels to the browser/renderer have been
  // created.
  void OnPpapiChannelsCreated(
      const IPC::ChannelHandle& ppapi_browser_channel_handle,
      const IPC::ChannelHandle& ppapi_renderer_channel_handle,
      const IPC::ChannelHandle& trusted_renderer_channel_handle,
      const IPC::ChannelHandle& manifest_service_channel_handle,
      base::ReadOnlySharedMemoryRegion crash_info_shmem_region);

  GURL manifest_url_;
  base::File nexe_file_;
  NaClFileToken nexe_token_;
  std::vector<NaClResourcePrefetchResult> prefetched_resource_files_;

  ppapi::PpapiPermissions permissions_;

  // The NaClHostMessageFilter that requested this NaCl process.  We use
  // this for sending the reply once the process has started.
  scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter_;

  // The reply message to send. We must always send this message when the
  // sub-process either succeeds or fails to unblock the renderer waiting for
  // the reply. NULL when there is no reply to send.
  raw_ptr<IPC::Message, AcrossTasksDanglingUntriaged> reply_msg_;

  // The file path to the manifest is passed to nacl-gdb when it is used to
  // debug the NaCl loader.
  base::FilePath manifest_path_;

  std::unique_ptr<content::BrowserChildProcessHost> process_;

  bool enable_debug_stub_;
  bool enable_crash_throttling_;
  bool off_the_record_;
  NaClAppProcessType process_type_;

  const base::FilePath profile_directory_;

  // Channel proxy to terminate the NaCl-Browser PPAPI channel.
  std::unique_ptr<IPC::ChannelProxy> ipc_proxy_channel_;
  // Browser host for plugin process.
  std::unique_ptr<content::BrowserPpapiHost> ppapi_host_;

  // Throttling time in milliseconds for PpapiHostMsg_Keepalive IPCs.
  static unsigned keepalive_throttle_interval_milliseconds_;

  base::WeakPtrFactory<NaClProcessHost> weak_factory_{this};
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_PROCESS_HOST_H_
