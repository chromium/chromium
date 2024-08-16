// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "test_util.h"

#include <array>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/enclave/types.h"
#include "device/fido/fido_constants.h"
#include "net/base/port_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

std::pair<base::Process, uint16_t> StartWebAuthnEnclave(base::FilePath cwd) {
  base::FilePath data_root;
  CHECK(base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &data_root));
  const base::FilePath enclave_bin_path =
      data_root.AppendASCII("cloud_authenticator_test_service");
  base::LaunchOptions subprocess_opts;
  subprocess_opts.current_directory = cwd;

  std::optional<base::Process> enclave_process;
  uint16_t port;
  char port_str[6];

  for (int i = 0; i < 10; i++) {
#if BUILDFLAG(IS_WIN)
    HANDLE read_handle;
    HANDLE write_handle;
    SECURITY_ATTRIBUTES security_attributes;

    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;
    security_attributes.lpSecurityDescriptor = NULL;
    CHECK(CreatePipe(&read_handle, &write_handle, &security_attributes, 0));

    subprocess_opts.stdin_handle = INVALID_HANDLE_VALUE;
    subprocess_opts.stdout_handle = write_handle;
    subprocess_opts.stderr_handle = INVALID_HANDLE_VALUE;
    subprocess_opts.handles_to_inherit.push_back(write_handle);
    enclave_process = base::LaunchProcess(base::CommandLine(enclave_bin_path),
                                          subprocess_opts);
    CloseHandle(write_handle);
    CHECK(enclave_process->IsValid());

    DWORD read_bytes;
    CHECK(ReadFile(read_handle, port_str, sizeof(port_str), &read_bytes, NULL));
    CloseHandle(read_handle);
#else
    int fds[2];
    CHECK(!pipe(fds));
    subprocess_opts.fds_to_remap.emplace_back(fds[1], 1);
    enclave_process = base::LaunchProcess(base::CommandLine(enclave_bin_path),
                                          subprocess_opts);
    CHECK(enclave_process->IsValid());
    close(fds[1]);

    const ssize_t read_bytes =
        HANDLE_EINTR(read(fds[0], port_str, sizeof(port_str)));
    close(fds[0]);
#endif

    CHECK(read_bytes > 0);
    port_str[read_bytes - 1] = 0;
    unsigned u_port;
    CHECK(base::StringToUint(port_str, &u_port)) << port_str;
    port = base::checked_cast<uint16_t>(u_port);

    if (net::IsPortAllowedForScheme(port, "wss")) {
      break;
    }
    LOG(INFO) << "Port " << port << " not allowed. Trying again.";

    // The kernel randomly picked a port that Chromium will refuse to connect
    // to. Try again.
    enclave_process->Terminate(/*exit_code=*/1, /*wait=*/false);
  }

  return std::make_pair(std::move(*enclave_process), port);
}

device::enclave::ScopedEnclaveOverride TestWebAuthnEnclaveIdentity(
    uint16_t port) {
  constexpr std::array<uint8_t, device::kP256X962Length> kTestPublicKey = {
      0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
      0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
      0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
      0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
      0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
      0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
  };
  const std::string url = "ws://127.0.0.1:" + base::NumberToString(port);
  device::enclave::EnclaveIdentity identity;
  identity.url = GURL(url);
  identity.public_key = kTestPublicKey;

  return device::enclave::ScopedEnclaveOverride(std::move(identity));
}

std::unique_ptr<device::cablev2::Pairing> TestPhone(const char* name,
                                                    uint8_t public_key,
                                                    base::Time last_updated,
                                                    int channel_priority) {
  auto phone = std::make_unique<device::cablev2::Pairing>();
  phone->name = name;
  phone->contact_id = {10, 11, 12};
  phone->id = {4, 5, 6};
  std::fill(phone->peer_public_key_x962.begin(),
            phone->peer_public_key_x962.end(), public_key);
  phone->last_updated = last_updated;
  phone->channel_priority = channel_priority;
  phone->from_sync_deviceinfo = true;
  return phone;
}
