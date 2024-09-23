// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_block_check.h"

#include <netlistmgr.h>
#include <wrl/client.h>

#include <utility>

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_version.h"
#include "chrome/updater/update_service.h"

namespace updater {
namespace {

// Returns true in situations where we allow background updates on metered
// networks.
bool AllowBackgroundUpdatesOnMeteredNetwork() {
  // TODO(crbug.com/40199605): Modify this function to enable background updates
  // on metered networks when a toggle is set in the browser.
  return true;
}

bool IsConnectionedMetered() {
  // No NLM before Win 8.1. Connections will be considered non-metered. Also,
  // because NLM could deadlock in Win10 versions pre-RS5, don't run the code
  // for those versions (see crbug.com/1254492).
  if (base::win::GetVersion() < base::win::Version::WIN10_RS5) {
    return false;
  }

  Microsoft::WRL::ComPtr<INetworkCostManager> network_cost_manager;
  HRESULT hr = ::CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&network_cost_manager));
  if (FAILED(hr)) {
    return false;
  }

  DWORD cost = NLM_CONNECTION_COST_UNKNOWN;
  hr = network_cost_manager->GetCost(&cost, nullptr);
  if (FAILED(hr)) {
    return false;
  }

  return cost != NLM_CONNECTION_COST_UNKNOWN &&
         (cost & NLM_CONNECTION_COST_UNRESTRICTED) == 0;
}

}  // namespace

void ShouldBlockUpdateForMeteredNetwork(
    UpdateService::Priority priority,
    base::OnceCallback<void(bool)> callback) {
  if (priority == UpdateService::Priority::kForeground ||
      AllowBackgroundUpdatesOnMeteredNetwork()) {
    std::move(callback).Run(false);
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&IsConnectionedMetered),
        std::move(callback));
  }
}

}  // namespace updater
