// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/nacl/common/nacl_types.h"
#include "ipc/ipc_platform_file.h"

namespace nacl {

NaClStartParams::NaClStartParams()
    : nexe_file(IPC::InvalidPlatformFileForTransit()),
      irt_handle(IPC::InvalidPlatformFileForTransit()),
#if BUILDFLAG(IS_POSIX)
      debug_stub_server_bound_socket(IPC::InvalidPlatformFileForTransit()),
#endif
      validation_cache_enabled(false),
      enable_debug_stub(false),
      process_type(kUnknownNaClProcessType) {
}

NaClStartParams::NaClStartParams(NaClStartParams&& other) = default;

NaClStartParams::~NaClStartParams() {
}

NaClResourcePrefetchResult::NaClResourcePrefetchResult()
    : file(IPC::InvalidPlatformFileForTransit()) {
}

NaClResourcePrefetchResult::NaClResourcePrefetchResult(
    const IPC::PlatformFileForTransit& file,
    const base::FilePath& file_path_metadata,
    const std::string& file_key)
    : file(file), file_path_metadata(file_path_metadata), file_key(file_key) {
}

NaClResourcePrefetchResult::~NaClResourcePrefetchResult() {
}

NaClResourcePrefetchRequest::NaClResourcePrefetchRequest() {
}

NaClResourcePrefetchRequest::NaClResourcePrefetchRequest(
    const std::string& file_key,
    const std::string& resource_url)
    : file_key(file_key),
      resource_url(resource_url) {
}

NaClResourcePrefetchRequest::~NaClResourcePrefetchRequest() {
}

NaClLaunchParams::NaClLaunchParams() = default;

NaClLaunchParams::NaClLaunchParams(
    const std::string& manifest_url,
    const IPC::PlatformFileForTransit& nexe_file,
    uint64_t nexe_token_lo,
    uint64_t nexe_token_hi,
    const std::vector<NaClResourcePrefetchRequest>&
        resource_prefetch_request_list,
    int render_frame_id,
    uint32_t permission_bits,
    NaClAppProcessType process_type)
    : manifest_url(manifest_url),
      nexe_file(nexe_file),
      nexe_token_lo(nexe_token_lo),
      nexe_token_hi(nexe_token_hi),
      resource_prefetch_request_list(resource_prefetch_request_list),
      render_frame_id(render_frame_id),
      permission_bits(permission_bits),
      process_type(process_type) {}

NaClLaunchParams::NaClLaunchParams(const NaClLaunchParams& other) = default;

NaClLaunchParams::~NaClLaunchParams() {
}

NaClLaunchResult::NaClLaunchResult()
    : ppapi_ipc_channel_handle(),
      trusted_ipc_channel_handle(),
      plugin_pid(base::kNullProcessId),
      plugin_child_id(0) {}

NaClLaunchResult::NaClLaunchResult(
    const IPC::ChannelHandle& ppapi_ipc_channel_handle,
    const IPC::ChannelHandle& trusted_ipc_channel_handle,
    const IPC::ChannelHandle& manifest_service_ipc_channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id,
    base::ReadOnlySharedMemoryRegion crash_info_shmem_region)
    : ppapi_ipc_channel_handle(ppapi_ipc_channel_handle),
      trusted_ipc_channel_handle(trusted_ipc_channel_handle),
      manifest_service_ipc_channel_handle(manifest_service_ipc_channel_handle),
      plugin_pid(plugin_pid),
      plugin_child_id(plugin_child_id),
      crash_info_shmem_region(std::move(crash_info_shmem_region)) {}

NaClLaunchResult::~NaClLaunchResult() {
}

}  // namespace nacl
