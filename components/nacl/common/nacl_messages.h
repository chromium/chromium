// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and NaCl process.

// Multiply-included message file, no traditional include guard.

#include <stdint.h>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/common/nacl_types_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/system/message_pipe.h"

#define IPC_MESSAGE_START NaClMsgStart

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClStartParams)
  IPC_STRUCT_TRAITS_MEMBER(nexe_file)
  IPC_STRUCT_TRAITS_MEMBER(nexe_file_path_metadata)
  IPC_STRUCT_TRAITS_MEMBER(irt_handle)
#if defined(OS_POSIX)
  IPC_STRUCT_TRAITS_MEMBER(debug_stub_server_bound_socket)
#endif
#if defined(OS_LINUX) || defined(OS_NACL_NONSFI)
  IPC_STRUCT_TRAITS_MEMBER(ppapi_browser_channel_handle)
  IPC_STRUCT_TRAITS_MEMBER(ppapi_renderer_channel_handle)
  IPC_STRUCT_TRAITS_MEMBER(trusted_service_channel_handle)
  IPC_STRUCT_TRAITS_MEMBER(manifest_service_channel_handle)
#endif
  IPC_STRUCT_TRAITS_MEMBER(validation_cache_enabled)
  IPC_STRUCT_TRAITS_MEMBER(validation_cache_key)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(enable_debug_stub)
  IPC_STRUCT_TRAITS_MEMBER(process_type)
  IPC_STRUCT_TRAITS_MEMBER(crash_info_shmem_region)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClResourcePrefetchResult)
  IPC_STRUCT_TRAITS_MEMBER(file)
  IPC_STRUCT_TRAITS_MEMBER(file_path_metadata)
  IPC_STRUCT_TRAITS_MEMBER(file_key)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// NaClProcess messages
// These are messages sent between the browser and the NaCl process.

// Sends a prefetched resource file to a NaCl loader process. This message
// can be sent multiple times, but all of them must be done before sending
// NaClProcessMsg_Start.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_AddPrefetchedResource,
                     nacl::NaClResourcePrefetchResult)

// Tells the NaCl process to start. This message can be sent only once.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_Start,
                     nacl::NaClStartParams /* params */)

#if defined(OS_WIN)
// Tells the NaCl broker to launch a NaCl loader process.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_LaunchLoaderThroughBroker,
                     int, /* launch_id */
                     mojo::MessagePipeHandle /* service_request_pipe */)

// Notify the browser process that the loader was launched successfully.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_LoaderLaunched,
                     int, /* launch_id */
                     base::ProcessHandle /* loader process handle */)

// Tells the NaCl broker to attach a debug exception handler to the
// given NaCl loader process.
IPC_MESSAGE_CONTROL3(NaClProcessMsg_LaunchDebugExceptionHandler,
                     int32_t /* pid of the NaCl process */,
                     base::ProcessHandle /* handle of the NaCl process */,
                     std::string /* NaCl internal process layout info */)

// Notify the browser process that the broker process finished
// attaching a debug exception handler to the given NaCl loader
// process.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_DebugExceptionHandlerLaunched,
                     int32_t /* pid */,
                     bool /* success */)

// Notify the broker that all loader processes have been terminated and it
// should shutdown.
IPC_MESSAGE_CONTROL0(NaClProcessMsg_StopBroker)

// Used by the NaCl process to request that a Windows debug exception
// handler be attached to it.
IPC_SYNC_MESSAGE_CONTROL1_1(NaClProcessMsg_AttachDebugExceptionHandler,
                            std::string, /* Internal process info */
                            bool /* Result */)

// Notify the browser process that the NaCl process has bound the given
// TCP port number to use for the GDB debug stub.
IPC_MESSAGE_CONTROL1(NaClProcessHostMsg_DebugStubPortSelected,
                     uint16_t /* debug_stub_port */)
#endif

// Used by the NaCl process to query a database in the browser.  The database
// contains the signatures of previously validated code chunks.
IPC_SYNC_MESSAGE_CONTROL1_1(NaClProcessMsg_QueryKnownToValidate,
                            std::string, /* A validation signature */
                            bool /* Can validation be skipped? */)

// Used by the NaCl process to add a validation signature to the validation
// database in the browser.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_SetKnownToValidate,
                     std::string /* A validation signature */)

// Used by the NaCl process to acquire trusted information about a file directly
// from the browser, including the file's path as well as a fresh version of the
// file handle.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_ResolveFileToken,
                     uint64_t, /* file_token_lo */
                     uint64_t /* file_token_hi */)
IPC_MESSAGE_CONTROL4(NaClProcessMsg_ResolveFileTokenReply,
                     uint64_t,                    /* file_token_lo */
                     uint64_t,                    /* file_token_hi */
                     IPC::PlatformFileForTransit, /* fd */
                     base::FilePath /* Path opened to get fd */)

// Notify the browser process that the server side of the PPAPI channel was
// created successfully.
// This is used for SFI mode only. Non-SFI mode passes channel handles in
// NaClStartParams instead.
IPC_MESSAGE_CONTROL5(
    NaClProcessHostMsg_PpapiChannelsCreated,
    IPC::ChannelHandle, /* browser_channel_handle */
    IPC::ChannelHandle, /* ppapi_renderer_channel_handle */
    IPC::ChannelHandle, /* trusted_renderer_channel_handle */
    IPC::ChannelHandle, /* manifest_service_channel_handle */
    base::ReadOnlySharedMemoryRegion /* crash_info_shmem_region */)
