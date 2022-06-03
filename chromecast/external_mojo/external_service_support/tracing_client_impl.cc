// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/external_mojo/external_service_support/tracing_client_impl.h"

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/trace_event/trace_event.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/traced_process.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace chromecast {
namespace external_service_support {

// static
const char TracingClient::kTracingServiceName[] = "external_tracing";

// static
std::unique_ptr<TracingClient> TracingClient::Create(
    ExternalConnector* connector) {
  return std::make_unique<TracingClientImpl>(connector);
}

TracingClientImpl::TracingClientImpl(ExternalConnector* connector)
    : connector_(connector) {
  DCHECK(connector_);

  // Setup this process.
  base::trace_event::TraceLog::GetInstance()->set_process_name(
      base::CommandLine::ForCurrentProcess()->GetProgram().value());
  // TODO(cletnick): Support initializing startup tracing without
  // depending on per-process command-line arguments.
  tracing::EnableStartupTracingIfNeeded();
  tracing::InitTracingPostThreadPoolStartAndFeatureList();

  // Connect to service.
  connector_->BindInterface(TracingClient::kTracingServiceName,
                            tracing_service_.BindNewPipeAndPassReceiver());
  // base::Unretained(this) is safe here because the mojo::Remote
  // |tracing_service_| is owned by |this| and will not run the registered
  // callback after destruction.
  tracing_service_.set_disconnect_handler(base::BindOnce(
      &TracingClientImpl::TracingServiceDisconnected, base::Unretained(this)));

  // Register with service.
  mojo::PendingRemote<tracing::mojom::TracedProcess> remote_process;
  tracing::TracedProcess::OnTracedProcessRequest(
      remote_process.InitWithNewPipeAndPassReceiver());
  tracing_service_->AddClient(tracing::mojom::ClientInfo::New(
      base::Process::Current().Pid(), std::move(remote_process)));
}

TracingClientImpl::~TracingClientImpl() = default;

void TracingClientImpl::TracingServiceDisconnected() {
  tracing_service_.reset();
  tracing::TracedProcess::ResetTracedProcessReceiver();
  LOG(ERROR) << "Disconnected from tracing service. Not reconnecting.";
}

}  // namespace external_service_support
}  // namespace chromecast
