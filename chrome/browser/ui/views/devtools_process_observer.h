// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEVTOOLS_PROCESS_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_DEVTOOLS_PROCESS_OBSERVER_H_

#include "base/process/process.h"
#include "content/public/browser/browser_child_process_observer.h"

namespace ui_devtools {
class TracingAgent;
}

// Observer that is notified of browser and gpu processes when they are launched
// and destroyed. This is used to update the process id for event tracing.
class DevtoolsProcessObserver : public content::BrowserChildProcessObserver {
 public:
  explicit DevtoolsProcessObserver(ui_devtools::TracingAgent* agent);
  ~DevtoolsProcessObserver() override;

 private:
  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  ui_devtools::TracingAgent* tracing_agent_;

  DISALLOW_COPY_AND_ASSIGN(DevtoolsProcessObserver);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEVTOOLS_PROCESS_OBSERVER_H_
