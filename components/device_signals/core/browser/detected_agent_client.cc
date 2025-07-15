// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/detected_agent_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/device_signals/core/common/signals_features.h"

namespace device_signals {

namespace {

std::optional<base::FilePath>& GetTestFilePathStorage() {
  static base::NoDestructor<std::optional<base::FilePath>> storage;
  return *storage;
}

base::FilePath GetCrowdStrikeAgentPath() {
  auto& storage = GetTestFilePathStorage();
  if (storage) {
    return storage.value();
  }

  return GetCrowdStrikeAgentInstallPath();
}

bool CheckAgentInstalled(Agents agent) {
  switch (agent) {
    case Agents::kCrowdStrikeFalcon:
      return base::PathExists(GetCrowdStrikeAgentPath());
  }
}

void GetDetectedAgents(base::OnceCallback<void(std::vector<Agents>)> callback) {
  std::vector<Agents> detectedAgents;
  for (int i = 0; i < static_cast<int>(Agents::kMaxValue) + 1; i++) {
    Agents agent = static_cast<Agents>(i);
    if (CheckAgentInstalled(agent)) {
      detectedAgents.push_back(agent);
    }
  }

  std::move(callback).Run(detectedAgents);
  return;
}

}  // namespace

using SignalsCallback = base::OnceCallback<void(std::vector<Agents>)>;

// static
void DetectedAgentClient::SetFilePathForTesting(
    const base::FilePath& file_path) {
  auto& storage = GetTestFilePathStorage();
  storage.emplace(file_path);
}

class DetectedAgentClientImpl : public DetectedAgentClient {
 public:
  DetectedAgentClientImpl();
  ~DetectedAgentClientImpl() override;

  // DetectedAgentClientImpl:
  void GetAgents(SignalsCallback callback) override;

 private:
  // Final function to be called in this flow with the `detected_agents_signal`
  // and will invoke the original caller's `callback`.
  void OnSignalsRetrieved(SignalsCallback callback,
                          std::vector<Agents> detected_agents_signal);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DetectedAgentClientImpl> weak_ptr_factory_{this};
};

// static
std::unique_ptr<DetectedAgentClient> DetectedAgentClient::Create() {
  return std::make_unique<DetectedAgentClientImpl>();
}

DetectedAgentClientImpl::DetectedAgentClientImpl() = default;

DetectedAgentClientImpl::~DetectedAgentClientImpl() = default;

void DetectedAgentClientImpl::GetAgents(SignalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enterprise_signals::features::IsDetectedAgentSignalCollectionEnabled()) {
    std::move(callback).Run({});
    return;
  }

  SignalsCallback result_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&DetectedAgentClientImpl::OnSignalsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetDetectedAgents, std::move(result_callback)));
}

void DetectedAgentClientImpl::OnSignalsRetrieved(
    SignalsCallback callback,
    std::vector<Agents> detected_agents_signal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run(std::move(detected_agents_signal));
}

}  // namespace device_signals
