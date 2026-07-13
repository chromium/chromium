// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/embedder.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/scoped_ipc_support.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/platform/platform_channel_endpoint.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/platform/platform_handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/invitation.h"
#include "chromeos/ash/components/mojo_proxy/service/node_proxy.h"
#include "chromeos/ash/components/mojo_proxy/service/portal_proxy.h"
#include "chromeos/ash/components/mojo_proxy/service/switches.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy {

void RunProxy(int argc, char** argv) {
  CHECK(base::CommandLine::Init(argc, argv));
  base::AtExitManager at_exit;
  logging::InitLogging({});
  logging::SetLogItems(true, true, true, true);

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  // Initialize the frozen legacy Mojo Core clone (`mojo_legacy`), which serves
  // the proxy's legacy client side. The real Mojo C APIs from //mojo are never
  // initialized nor used in this process: the legacy side goes through the
  // `mojo_legacy` clone, and the MojoIpcz side below is driven through direct
  // ipcz and ipcz driver calls.
  //
  // We always operate as a broker on the legacy side based on the assumption
  // that all legacy clients are non-brokers. We're the only node the legacy
  // client communicates with.
  mojo_legacy::core::Configuration mojo_config;
  mojo_config.is_broker_process = true;
  mojo_legacy::core::Init(mojo_config);
  at_exit.RegisterTask(base::BindOnce(&mojo_legacy::core::ShutDown));
  auto ipc_support = std::make_unique<mojo_legacy::core::ScopedIPCSupport>(
      io_task_executor.task_runner(),
      mojo_legacy::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  // The MojoIpcz driver's transports run their I/O on the same thread
  // as the legacy side (//mojo embedders usually get this from
  // `mojo::core::ScopedIPCSupport`, which this process does not use).
  mojo::core::ipcz_driver::Transport::SetIOTaskRunner(
      io_task_executor.task_runner());

  // Also initialize the global MojoIpcz node. Any interaction with the
  // MojoIpcz side of the proxy is done through direct calls into either ipcz
  // or the MojoIpcz driver.
  //
  // On the ipcz side we're a non-broker, based on the assumption that either
  // our ipcz client is a broker or (if --inherit-ipcz-broker is given) we can
  // inherit a broker from them.
  mojo::core::IpczNodeOptions ipcz_options{
      .is_broker = false,
      .use_local_shared_memory_allocation = true,
  };
  CHECK(mojo::core::InitializeIpczNodeForProcess(ipcz_options));
  const IpczHandle ipcz_node = mojo::core::GetIpczNode();

  int fd;
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  CHECK(base::StringToInt(
      command_line.GetSwitchValueASCII(switches::kLegacyClientFd), &fd));
  mojo_legacy::PlatformChannelEndpoint legacy_endpoint{
      mojo_legacy::PlatformHandle{base::ScopedFD{fd}}};
  CHECK(base::StringToInt(
      command_line.GetSwitchValueASCII(switches::kHostIpczTransportFd), &fd));
  mojo::PlatformChannelEndpoint ipcz_endpoint{
      mojo::PlatformHandle{base::ScopedFD{fd}}};

  // Some Mojo clients use free-form strings for attachment names, and some use
  // 64-bit integral, zero-based values. In general only the latter cases attach
  // multiple pipes to a single invitation. The chosen scheme influences how
  // MojoIpcz (and therefore how this proxy) maps attachment names from Mojo
  // APIs to an index into the portal array filled im by ipcz ConnectNode().
  std::vector<std::string> attachment_names;
  if (command_line.HasSwitch(switches::kAttachmentName)) {
    attachment_names.push_back(
        command_line.GetSwitchValueASCII(switches::kAttachmentName));
  } else if (command_line.HasSwitch(switches::kNumAttachments)) {
    uint64_t num_unnamed_attachments;
    CHECK(base::StringToUint64(
        command_line.GetSwitchValueASCII(switches::kNumAttachments),
        &num_unnamed_attachments));
    for (uint64_t i = 0; i < num_unnamed_attachments; ++i) {
      attachment_names.emplace_back(reinterpret_cast<const char*>(&i),
                                    sizeof(i));
    }
  }

  // Create an appropriate ipcz transport to connect back to the host.
  using Transport = mojo::core::ipcz_driver::Transport;
  const bool inherit_ipcz_broker =
      command_line.HasSwitch(switches::kInheritIpczBroker);
  const Transport::EndpointType ipcz_client_type =
      inherit_ipcz_broker ? Transport::kNonBroker : Transport::kBroker;
  auto ipcz_transport = Transport::Create(
      {.source = Transport::kNonBroker, .destination = ipcz_client_type},
      std::move(ipcz_endpoint), base::Process{});

  // Portal 0 is reserved (see below). The portals corresponding to invitation
  // attachments span indices [1, N].
  const IpczAPI& ipcz = mojo::core::GetIpczAPI();
  std::vector<IpczHandle> initial_portals(attachment_names.size() + 1);
  const IpczConnectNodeFlags connect_flags =
      inherit_ipcz_broker ? IPCZ_CONNECT_NODE_INHERIT_BROKER
                          : IPCZ_CONNECT_NODE_TO_BROKER;
  const IpczResult connect_result = ipcz.ConnectNode(
      ipcz_node, Transport::ReleaseAsHandle(std::move(ipcz_transport)),
      initial_portals.size(), connect_flags, nullptr, initial_portals.data());
  CHECK_EQ(IPCZ_RESULT_OK, connect_result);

  // Portal 0 is bound on the other end to an internal shared memory allocation
  // service by MojoIpcz. We don't need it.
  ipcz.Close(initial_portals[0], IPCZ_NO_FLAGS, nullptr);

  // Seed the server with proxies between each of the attached pipes on the
  // legacy invitation and their corresponding initial portals from the host
  // connection.
  base::RunLoop run_loop;
  NodeProxy proxy(ipcz, /*dead_callback=*/run_loop.QuitClosure());
  mojo_legacy::OutgoingInvitation invitation;
  for (size_t i = 0; i < attachment_names.size(); ++i) {
    proxy.AddPortalProxy(mojo::core::ScopedIpczHandle(initial_portals[i + 1]),
                         invitation.AttachMessagePipe(attachment_names[i]));
  }

  // After sending the legacy invitation, we wait until all proxies are dead.
  mojo_legacy::OutgoingInvitation::Send(std::move(invitation),
                                        base::kNullProcessHandle,
                                        std::move(legacy_endpoint));
  run_loop.Run();

  mojo::core::DestroyIpczNodeForProcess();
  ipc_support.reset();
}

}  // namespace mojo_proxy

int main(int argc, char** argv) {
  mojo_proxy::RunProxy(argc, argv);
  return 0;
}
