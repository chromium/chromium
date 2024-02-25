// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and NaCl process.

// no-include-guard-because-multiply-included
// Multiply-included message file, no traditional include guard.

#include <stdint.h>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/common/nacl_types_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/system/message_pipe.h"

#define IPC_MESSAGE_START NaClMsgStart

IPC_STRUCT_TRAITS_BEGIN(nacl::NaClStartParams)
  IPC_STRUCT_TRAITS_MEMBER(nexe_file)
  IPC_STRUCT_TRAITS_MEMBER(nexe_file_path_metadata)
  IPC_STRUCT_TRAITS_MEMBER(irt_handle)
#if BUILDFLAG(IS_POSIX)
  IPC_STRUCT_TRAITS_MEMBER(debug_stub_server_bound_socket)
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
IPC_MESSAGE_CONTROL5(
    NaClProcessHostMsg_PpapiChannelsCreated,
    IPC::ChannelHandle, /* browser_channel_handle */
    IPC::ChannelHandle, /* ppapi_renderer_channel_handle */
    IPC::ChannelHandle, /* trusted_renderer_channel_handle */
    IPC::ChannelHandle, /* manifest_service_channel_handle */
    base::ReadOnlySharedMemoryRegion /* crash_info_shmem_region */)
