// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/server_holder.h"

#include "base/path_service.h"
#include "components/ui_devtools/connector_delegate.h"
#include "components/ui_devtools/devtools_process_observer.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/views/devtools_server_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_service.h"

namespace ui_devtools {

namespace {
// This connector is used in ui_devtools's TracingAgent to hook up with the
// tracing service.
class UiDevtoolsConnector : public ui_devtools::ConnectorDelegate {
 public:
  UiDevtoolsConnector() = default;
  ~UiDevtoolsConnector() override = default;

  void BindTracingConsumerHost(
      mojo::PendingReceiver<tracing::mojom::ConsumerHost> receiver) override {
    content::GetTracingService().BindConsumerHost(std::move(receiver));
  }
};
}  // namespace

ServerHolder::ServerHolder() = default;

ServerHolder::~ServerHolder() = default;

// static
ServerHolder* ServerHolder::GetInstance() {
  static base::NoDestructor<ServerHolder> instance;
  return instance.get();
}

void ServerHolder::CreateUiDevTools(
    const base::FilePath& active_port_output_directory) {
  DCHECK(!devtools_server_);
  DCHECK(!devtools_process_observer_);

  // Starts the UI Devtools server for browser UI (and Ash UI on Chrome OS).
  auto connector = std::make_unique<UiDevtoolsConnector>();
  devtools_server_ = ui_devtools::CreateUiDevToolsServerForViews(
      content::GetIOThreadTaskRunner(), std::move(connector),
      active_port_output_directory);
  devtools_process_observer_ = std::make_unique<DevtoolsProcessObserver>(
      devtools_server_->tracing_agent());
}

const UiDevToolsServer* ServerHolder::GetUiDevToolsServerInstance() {
  return devtools_server_.get();
}

void ServerHolder::DestroyUiDevTools() {
  devtools_server_.reset();
  devtools_process_observer_.reset();
}

}  // namespace ui_devtools
