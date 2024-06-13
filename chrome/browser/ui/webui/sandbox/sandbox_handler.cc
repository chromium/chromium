// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sandbox/sandbox_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/process_type.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/win/sandbox_win.h"

using content::BrowserChildProcessHostIterator;
using content::ChildProcessData;
using content::RenderProcessHost;

namespace sandbox_handler {
namespace {

base::Value::List FetchBrowserChildProcesses() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::List browser_processes;

  for (BrowserChildProcessHostIterator itr; !itr.Done(); ++itr) {
    const ChildProcessData& process_data = itr.GetData();
    // Only add processes that have already started, i.e. with valid handles.
    if (!process_data.GetProcess().IsValid())
      continue;
    base::Value::Dict proc;
    proc.Set("processId",
             base::strict_cast<double>(process_data.GetProcess().Pid()));
    proc.Set("processType",
             content::GetProcessTypeNameInEnglish(process_data.process_type));
    proc.Set("name", process_data.name);
    proc.Set("metricsName", process_data.metrics_name);
    proc.Set("sandboxType",
             sandbox::policy::SandboxWin::GetSandboxTypeInEnglish(
                 process_data.sandbox_type));
    browser_processes.Append(std::move(proc));
  }

  return browser_processes;
}

base::Value::List FetchRenderHostProcesses() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::List renderer_processes;

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    // Skip processes that might not have started yet.
    if (!host->GetProcess().IsValid())
      continue;

    base::Value::Dict proc;
    proc.Set("processId", base::strict_cast<double>(host->GetProcess().Pid()));
    renderer_processes.Append(std::move(proc));
  }

  return renderer_processes;
}

base::Value::Dict FeatureToValue(const base::Feature& feature) {
  base::Value::Dict feature_info;
  feature_info.Set("name", feature.name);
  feature_info.Set("enabled", base::FeatureList::IsEnabled(feature));
  return feature_info;
}

base::Value::List FetchSandboxFeatures() {
  base::Value::List features;
  features.Append(FeatureToValue(sandbox::policy::features::kGpuAppContainer));
  features.Append(FeatureToValue(sandbox::policy::features::kGpuLPAC));
  features.Append(
      FeatureToValue(sandbox::policy::features::kNetworkServiceSandbox));
  features.Append(
      FeatureToValue(sandbox::policy::features::kRendererAppContainer));
  features.Append(FeatureToValue(
      sandbox::policy::features::kWinSboxDisableExtensionPoints));
  features.Append(
      FeatureToValue(sandbox::policy::features::kWinSboxZeroAppShim));
  features.Append(
      FeatureToValue(sandbox::policy::features::kWinSboxNoFakeGdiInit));
  features.Append(FeatureToValue(
      sandbox::policy::features::kWinSboxRestrictCoreSharingOnRenderer));
  return features;
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
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK_EQ(1U, args.size());
  sandbox_diagnostics_callback_id_ = args[0].Clone();

  AllowJavascript();

  browser_processes_ = FetchBrowserChildProcesses();

  sandbox::policy::SandboxWin::GetPolicyDiagnostics(
      base::BindOnce(&SandboxHandler::FetchSandboxDiagnosticsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

// This runs nested inside SandboxWin so we get out quickly.
void SandboxHandler::FetchSandboxDiagnosticsCompleted(
    base::Value sandbox_policies) {
  sandbox_policies_ = std::move(sandbox_policies);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SandboxHandler::GetRendererProcessesAndFinish,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SandboxHandler::GetRendererProcessesAndFinish() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto renderer_processes = FetchRenderHostProcesses();
  base::Value::Dict results;
  results.Set("browser", std::move(browser_processes_));
  results.Set("policies", std::move(sandbox_policies_));
  results.Set("renderer", std::move(renderer_processes));
  results.Set("features", FetchSandboxFeatures());
  ResolveJavascriptCallback(sandbox_diagnostics_callback_id_,
                            std::move(results));
}

}  // namespace sandbox_handler
