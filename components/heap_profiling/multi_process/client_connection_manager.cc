// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/multi_process/client_connection_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_service.mojom.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace heap_profiling {

namespace {

bool ShouldProfileNonRendererProcessType(Mode mode, int process_type) {
  switch (mode) {
    case Mode::kAll:
      return true;

    case Mode::kAllRenderers:
      // Renderer logic is handled in ClientConnectionManager::Observe.
      return false;

    case Mode::kManual:
      return false;

    case Mode::kMinimal:
      return (process_type == content::ProcessType::PROCESS_TYPE_GPU ||
              process_type == content::ProcessType::PROCESS_TYPE_BROWSER);

    case Mode::kGpu:
      return process_type == content::ProcessType::PROCESS_TYPE_GPU;

    case Mode::kBrowser:
      return process_type == content::ProcessType::PROCESS_TYPE_BROWSER;

    case Mode::kRendererSampling:
      // Renderer logic is handled in ClientConnectionManager::Observe.
      return false;

    case Mode::kUtilitySampling:
      // Sample each utility process with 1/3 probability.
      if (process_type == content::ProcessType::PROCESS_TYPE_UTILITY)
        return (base::RandUint64() % 3) < 1;
      return false;

    case Mode::kUtilityAndBrowser:
      return process_type == content::ProcessType::PROCESS_TYPE_UTILITY ||
             process_type == content::ProcessType::PROCESS_TYPE_BROWSER;

    case Mode::kNone:
      return false;

    case Mode::kCount:
      // Fall through to hit NOTREACHED() below.
      {}
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

void StartProfilingClientOnIOThread(
    base::WeakPtr<Controller> controller,
    mojo::PendingRemote<mojom::ProfilingClient> client,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (!controller)
    return;

  controller->StartProfilingClient(std::move(client), pid, process_type,
                                   std::move(started_profiling_closure));
}

void StartProfilingBrowserProcessOnIOThread(
    base::WeakPtr<Controller> controller,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (!controller)
    return;

  static base::NoDestructor<ProfilingClient> client;
  mojo::PendingRemote<mojom::ProfilingClient> remote;
  client->BindToInterface(remote.InitWithNewPipeAndPassReceiver());
  controller->StartProfilingClient(std::move(remote), base::GetCurrentProcId(),
                                   mojom::ProcessType::BROWSER,
                                   std::move(started_profiling_closure));
}

}  // namespace

ClientConnectionManager::ClientConnectionManager(
    base::WeakPtr<Controller> controller,
    Mode mode)
    : controller_(controller), mode_(mode) {}

ClientConnectionManager::~ClientConnectionManager() {
  Remove(this);
}

void ClientConnectionManager::Start() {
  Add(this);

  StartProfilingExistingProcessesIfNecessary();
}

Mode ClientConnectionManager::GetMode() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return mode_;
}

void ClientConnectionManager::StartProfilingProcess(
    base::ProcessId pid,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  mode_ = Mode::kManual;

  // The RenderProcessHost iterator must be used on the UI thread.
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (pid == iter.GetCurrentValue()->GetProcess().Pid()) {
      StartProfilingRenderer(iter.GetCurrentValue(),
                             std::move(started_profiling_closure));
      return;
    }
  }

  // Check if the request is for the current process.
  if (pid == base::GetCurrentProcId()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&StartProfilingBrowserProcessOnIOThread, controller_,
                       std::move(started_profiling_closure)));
    return;
  }

  // Check if the request is for a non-renderer child process.
  for (content::BrowserChildProcessHostIterator browser_child_iter;
       !browser_child_iter.Done(); ++browser_child_iter) {
    const content::ChildProcessData& data = browser_child_iter.GetData();
    if (data.GetProcess().Pid() == pid) {
      StartProfilingNonRendererChild(data,
                                     std::move(started_profiling_closure));
      return;
    }
  }

  DLOG(WARNING)
      << "Attempt to start profiling failed as no process was found with pid: "
      << pid;
}

bool ClientConnectionManager::AllowedToProfileRenderer(
    content::RenderProcessHost* host) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return true;
}

void ClientConnectionManager::SetModeForTesting(Mode mode) {
  mode_ = mode;
}

void ClientConnectionManager::StartProfilingExistingProcessesIfNecessary() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Start profiling the current process.
  if (ShouldProfileNonRendererProcessType(
          mode_, content::ProcessType::PROCESS_TYPE_BROWSER)) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&StartProfilingBrowserProcessOnIOThread,
                                  controller_, base::DoNothing()));
  }

  // Start profiling connected renderers.
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (ShouldProfileNewRenderer(iter.GetCurrentValue()) &&
        iter.GetCurrentValue()->GetProcess().Handle() !=
            base::kNullProcessHandle) {
      StartProfilingRenderer(iter.GetCurrentValue(), base::DoNothing());
    }
  }

  for (content::BrowserChildProcessHostIterator browser_child_iter;
       !browser_child_iter.Done(); ++browser_child_iter) {
    const content::ChildProcessData& data = browser_child_iter.GetData();
    if (ShouldProfileNonRendererProcessType(mode_, data.process_type) &&
        data.GetProcess().IsValid()) {
      StartProfilingNonRendererChild(data, base::DoNothing());
    }
  }
}

void ClientConnectionManager::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ensure this is only called for all non-renderer browser child processes
  // so as not to collide with logic in OnRenderProcessHostCreated().
  DCHECK_NE(data.process_type, content::ProcessType::PROCESS_TYPE_RENDERER);

  if (!ShouldProfileNonRendererProcessType(mode_, data.process_type))
    return;

  StartProfilingNonRendererChild(data, base::DoNothing());
}

void ClientConnectionManager::StartProfilingNonRendererChild(
    const content::ChildProcessData& data,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(data.id);
  if (!host)
    return;

  mojom::ProcessType process_type =
      (data.process_type == content::ProcessType::PROCESS_TYPE_GPU)
          ? mojom::ProcessType::GPU
          : mojom::ProcessType::OTHER;

  // Tell the child process to start profiling.
  mojo::PendingRemote<mojom::ProfilingClient> client;
  host->GetHost()->BindReceiver(client.InitWithNewPipeAndPassReceiver());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&StartProfilingClientOnIOThread, controller_,
                     std::move(client), data.GetProcess().Pid(), process_type,
                     std::move(started_profiling_closure)));
}

void ClientConnectionManager::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (ShouldProfileNewRenderer(host)) {
    StartProfilingRenderer(host, base::DoNothing());
    if (!host_observation_.IsObservingSource(host)) {
      host_observation_.AddObservation(host);
    }
  }
}

void ClientConnectionManager::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  profiled_renderers_.erase(host);
  host_observation_.RemoveObservation(host);
}

// RenderProcessHostDestroyed() will be invoked only if RenderProcessExited()
// was not, since we remove the observation of `host` in that function.
void ClientConnectionManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  profiled_renderers_.erase(host);
  host_observation_.RemoveObservation(host);
}

bool ClientConnectionManager::ShouldProfileNewRenderer(
    content::RenderProcessHost* renderer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Allow subclasses to not profile renderers.
  if (!AllowedToProfileRenderer(renderer))
    return false;

  Mode mode = GetMode();
  if (mode == Mode::kAll || mode == Mode::kAllRenderers) {
    return true;
  } else if (mode == Mode::kRendererSampling && profiled_renderers_.empty()) {
    // Sample renderers with a 1/3 probability.
    return (base::RandUint64() % 100000) < 33333;
  }

  return false;
}

void ClientConnectionManager::StartProfilingRenderer(
    content::RenderProcessHost* host,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  profiled_renderers_.insert(host);

  mojo::PendingRemote<mojom::ProfilingClient> client;
  host->BindReceiver(client.InitWithNewPipeAndPassReceiver());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&StartProfilingClientOnIOThread, controller_,
                                std::move(client), host->GetProcess().Pid(),
                                mojom::ProcessType::RENDERER,
                                std::move(started_profiling_closure)));
}

}  // namespace heap_profiling
