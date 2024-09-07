// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DEVTOOLS_SERVER_UTIL_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DEVTOOLS_SERVER_UTIL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/ui_devtools/connector_delegate.h"
#include "components/ui_devtools/devtools_server.h"

namespace ui_devtools {

// A factory helper to construct a UiDevToolsServer for Views.
// The connector is used in TracingAgent to hook up with the tracing service.
std::unique_ptr<UiDevToolsServer> CreateUiDevToolsServerForViews(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    std::unique_ptr<ConnectorDelegate> connector,
    const base::FilePath& active_port_output_directory);

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_DEVTOOLS_SERVER_UTIL_H_
