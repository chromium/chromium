// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
// no-include-guard-because-multiply-included

#include <stdint.h>

#include <string>

#include "base/process/process.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/common/nacl_types_param_traits.h"
#include "components/nacl/common/pnacl_types.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_platform_file.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START NaClHostMsgStart

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClResourcePrefetchRequest)
  IPC_STRUCT_TRAITS_MEMBER(file_key)
  IPC_STRUCT_TRAITS_MEMBER(resource_url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClLaunchParams)
  IPC_STRUCT_TRAITS_MEMBER(manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(nexe_file)
  IPC_STRUCT_TRAITS_MEMBER(nexe_token_lo)
  IPC_STRUCT_TRAITS_MEMBER(nexe_token_hi)
  IPC_STRUCT_TRAITS_MEMBER(resource_prefetch_request_list)
  IPC_STRUCT_TRAITS_MEMBER(render_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(permission_bits)
  IPC_STRUCT_TRAITS_MEMBER(process_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClLaunchResult)
  IPC_STRUCT_TRAITS_MEMBER(ppapi_ipc_channel_handle)
  IPC_STRUCT_TRAITS_MEMBER(trusted_ipc_channel_handle)
  IPC_STRUCT_TRAITS_MEMBER(manifest_service_ipc_channel_handle)
  IPC_STRUCT_TRAITS_MEMBER(plugin_pid)
  IPC_STRUCT_TRAITS_MEMBER(plugin_child_id)
  IPC_STRUCT_TRAITS_MEMBER(crash_info_shmem_region)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(nacl::PnaclCacheInfo)
  IPC_STRUCT_TRAITS_MEMBER(pexe_url)
  IPC_STRUCT_TRAITS_MEMBER(abi_version)
  IPC_STRUCT_TRAITS_MEMBER(opt_level)
  IPC_STRUCT_TRAITS_MEMBER(last_modified)
  IPC_STRUCT_TRAITS_MEMBER(etag)
  IPC_STRUCT_TRAITS_MEMBER(has_no_store_header)
  IPC_STRUCT_TRAITS_MEMBER(use_subzero)
  IPC_STRUCT_TRAITS_MEMBER(sandbox_isa)
  IPC_STRUCT_TRAITS_MEMBER(extra_flags)
IPC_STRUCT_TRAITS_END()

// A renderer sends this to the browser process when it wants to start
// a new instance of the Native Client process. The browser will launch
// the process and return an IPC channel handle. This handle will only
// be valid if the NaCl IPC proxy is enabled.
IPC_SYNC_MESSAGE_CONTROL1_2(NaClHostMsg_LaunchNaCl,
                            nacl::NaClLaunchParams /* launch_params */,
                            nacl::NaClLaunchResult /* launch_result */,
                            std::string /* error_message */)

// A renderer sends this to the browser process when it wants to
// open a file for from the Pnacl component directory.
IPC_SYNC_MESSAGE_CONTROL2_3(NaClHostMsg_GetReadonlyPnaclFD,
                            std::string /* name of requested PNaCl file */,
                            bool /* is_executable */,
                            IPC::PlatformFileForTransit /* output file */,
                            uint64_t /* file_token_lo */,
                            uint64_t /* file_token_hi */)

// A renderer sends this to the browser process when it wants to
// create a temporary file.
IPC_SYNC_MESSAGE_CONTROL0_1(NaClHostMsg_NaClCreateTemporaryFile,
                            IPC::PlatformFileForTransit /* out file */)

// A renderer sends this to the browser to request a file descriptor for
// a translated nexe.
IPC_MESSAGE_CONTROL2(NaClHostMsg_NexeTempFileRequest,
                     int /* instance */,
                     nacl::PnaclCacheInfo /* cache info */)

// The browser replies to a renderer's temp file request with output_file,
// which is either a writeable temp file to use for translation, or a
// read-only file containing the translated nexe from the cache.
IPC_MESSAGE_CONTROL3(NaClViewMsg_NexeTempFileReply,
                     int /* instance */,
                     bool /* is_cache_hit */,
                     IPC::PlatformFileForTransit /* output file */)

// A renderer sends this to the browser to report that its translation has
// finished and its temp file contains the translated nexe.
IPC_MESSAGE_CONTROL2(NaClHostMsg_ReportTranslationFinished,
                     int /* instance */,
                     bool /* success */)

// A renderer sends this to the browser process to report when the client
// architecture is not listed in the manifest.
IPC_MESSAGE_CONTROL1(NaClHostMsg_MissingArchError, int /* render_frame_id */)

// A renderer sends this to the browser process when it wants to
// open a NaCl executable file from an installed application directory.
IPC_SYNC_MESSAGE_CONTROL2_3(NaClHostMsg_OpenNaClExecutable,
                            int /* render_frame_id */,
                            GURL /* URL of NaCl executable file */,
                            IPC::PlatformFileForTransit /* output file */,
                            uint64_t /* file_token_lo */,
                            uint64_t /* file_token_hi */)

// A renderer sends this to the browser process to determine how many
// processors are online.
IPC_SYNC_MESSAGE_CONTROL0_1(NaClHostMsg_NaClGetNumProcessors,
                            int /* Number of processors */)

// A renderer sends this to the browser process to determine if the
// NaCl application started from the given NMF URL will be debugged.
// If not (filtered out by commandline flags), it sets should_debug to false.
IPC_SYNC_MESSAGE_CONTROL1_1(NaClHostMsg_NaClDebugEnabledForURL,
                            GURL /* alleged URL of NMF file */,
                            bool /* should debug */)
