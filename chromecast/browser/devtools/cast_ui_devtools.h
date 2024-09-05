// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_CAST_UI_DEVTOOLS_H_
#define CHROMECAST_BROWSER_DEVTOOLS_CAST_UI_DEVTOOLS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

namespace ui_devtools {
class UiDevToolsServer;
}  // namespace ui_devtools

namespace chromecast {
namespace shell {

// Container to instantiate a separate devtools server for viewing the hierarchy
// of the Aura UI tree. Instantiated only when USE_AURA is true and
// --enable-ui-devtools is passed.
class CastUIDevTools {
 public:
  explicit CastUIDevTools(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);

  CastUIDevTools(const CastUIDevTools&) = delete;
  CastUIDevTools& operator=(const CastUIDevTools&) = delete;

  ~CastUIDevTools();

 private:
  std::unique_ptr<ui_devtools::UiDevToolsServer> CreateServer(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner) const;

  std::unique_ptr<ui_devtools::UiDevToolsServer> devtools_server_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_CAST_UI_DEVTOOLS_H_
