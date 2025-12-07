// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_SERVER_HOLDER_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_SERVER_HOLDER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/no_destructor.h"

class DevtoolsProcessObserver;

namespace ui_devtools {

class UiDevToolsServer;

class ServerHolder {
 public:
  ServerHolder(const ServerHolder&) = delete;
  ServerHolder& operator=(const ServerHolder&) = delete;

  static ServerHolder* GetInstance();

  // For details, refer to the documentation of
  // UiDevToolsServer::CreateForViews().
  void CreateUiDevTools(const base::FilePath& active_port_output_directory);
  const UiDevToolsServer* GetUiDevToolsServerInstance();
  void DestroyUiDevTools();

 private:
  friend class base::NoDestructor<ServerHolder>;
  ServerHolder();
  ~ServerHolder();

  std::unique_ptr<ui_devtools::UiDevToolsServer> devtools_server_;
  std::unique_ptr<DevtoolsProcessObserver> devtools_process_observer_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_SERVER_HOLDER_H_
