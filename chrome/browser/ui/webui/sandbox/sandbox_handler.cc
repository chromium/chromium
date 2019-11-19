// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sandbox/sandbox_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/process_type.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"

using content::BrowserChildProcessHostIterator;
using content::ChildProcessData;
using content::RenderProcessHost;

namespace sandbox_handler {
namespace {
base::Value FetchBrowserChildProcesses() {
  // The |BrowserChildProcessHostIterator| must only be used on the IO thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::Value browser_processes(base::Value::Type::LIST);

  for (BrowserChildProcessHostIterator itr; !itr.Done(); ++itr) {
    const ChildProcessData& process_data = itr.GetData();
    // Only add processes that have already started, i.e. with valid handles.
    if (!process_data.GetProcess().IsValid())
      continue;
    base::Value proc(base::Value::Type::DICTIONARY);
    proc.SetPath("processId", base::Value(base::strict_cast<double>(
                                  process_data.GetProcess().Pid())));
    proc.SetPath("processType",
                 base::Value(content::GetProcessTypeNameInEnglish(
                     process_data.process_type)));
    proc.SetPath("name", base::Value(process_data.name));
    proc.SetPath("metricsName", base::Value(process_data.metrics_name));
    browser_processes.GetList().push_back(std::move(proc));
  }

  return browser_processes;
}

base::Value FetchRenderHostProcesses() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value renderer_processes(base::Value::Type::LIST);

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    // Skip processes that might not have started yet.
    if (!host->GetProcess().IsValid())
      continue;

    base::Value proc(base::Value::Type::DICTIONARY);
    proc.SetPath(
        "processId",
        base::Value(base::strict_cast<double>(host->GetProcess().Pid())));
    renderer_processes.GetList().push_back(std::move(proc));
  }

  return renderer_processes;
}

}  // namespace

SandboxHandler::SandboxHandler() = default;
SandboxHandler::~SandboxHandler() = default;

void SandboxHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "requestSandboxDiagnostics",
      base::BindRepeating(&SandboxHandler::HandleRequestSandboxDiagnostics,
                          base::Unretained(this)));
}

void SandboxHandler::HandleRequestSandboxDiagnostics(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK_EQ(1U, args->GetList().size());
  sandbox_diagnostics_callback_id_ = args->GetList()[0].Clone();

  AllowJavascript();

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&FetchBrowserChildProcesses),
      base::Bind(&SandboxHandler::FetchBrowserChildProcessesCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SandboxHandler::FetchBrowserChildProcessesCompleted(
    base::Value browser_processes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  browser_processes_ = std::move(browser_processes);

  service_manager::SandboxWin::GetPolicyDiagnostics(
      base::Bind(&SandboxHandler::FetchSandboxDiagnosticsCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

// This runs nested inside SandboxWin so we get out quickly.
void SandboxHandler::FetchSandboxDiagnosticsCompleted(
    base::Value sandbox_policies) {
  sandbox_policies_ = std::move(sandbox_policies);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&SandboxHandler::GetRendererProcessesAndFinish,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SandboxHandler::GetRendererProcessesAndFinish() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto renderer_processes = FetchRenderHostProcesses();
  base::Value results(base::Value::Type::DICTIONARY);
  results.SetPath("browser", std::move(browser_processes_));
  results.SetPath("policies", std::move(sandbox_policies_));
  results.SetPath("renderer", std::move(renderer_processes));
  ResolveJavascriptCallback(sandbox_diagnostics_callback_id_,
                            std::move(results));
}

}  // namespace sandbox_handler
