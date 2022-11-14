// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/remote_cdm_context.h"

#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/cdm/cdm_context_ref_impl.h"

namespace chromeos {

namespace {
class RemoteCdmContextRef final : public media::CdmContextRef {
 public:
  explicit RemoteCdmContextRef(scoped_refptr<RemoteCdmContext> cdm_context)
      : cdm_context_(std::move(cdm_context)) {}

  RemoteCdmContextRef(const RemoteCdmContextRef&) = delete;
  RemoteCdmContextRef& operator=(const RemoteCdmContextRef&) = delete;

  ~RemoteCdmContextRef() final = default;

  // CdmContextRef:
  media::CdmContext* GetCdmContext() final { return cdm_context_.get(); }

 private:
  scoped_refptr<RemoteCdmContext> cdm_context_;
};
}  // namespace

RemoteCdmContext::RemoteCdmContext(
    mojo::PendingRemote<media::stable::mojom::StableCdmContext>
        stable_cdm_context)
    : stable_cdm_context_(std::move(stable_cdm_context)),
      mojo_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

std::unique_ptr<media::CallbackRegistration> RemoteCdmContext::RegisterEventCB(
    EventCB event_cb) {
  auto registration = event_callbacks_.Register(std::move(event_cb));
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoteCdmContext::RegisterForRemoteCallbacks,
                                weak_ptr_factory_.GetWeakPtr()));
  return registration;
}

RemoteCdmContext::~RemoteCdmContext() {}

void RemoteCdmContext::RegisterForRemoteCallbacks() {
  if (!event_callback_receiver_.is_bound()) {
    stable_cdm_context_->RegisterEventCallback(
        event_callback_receiver_.BindNewPipeAndPassRemote());
  }
}

ChromeOsCdmContext* RemoteCdmContext::GetChromeOsCdmContext() {
  return this;
}

void RemoteCdmContext::GetHwKeyData(const media::DecryptConfig* decrypt_config,
                                    const std::vector<uint8_t>& hw_identifier,
                                    GetHwKeyDataCB callback) {
  // Clone the |decrypt_config| in case the pointer becomes invalid when we are
  // re-posting the task.
  GetHwKeyDataInternal(decrypt_config->Clone(), hw_identifier,
                       std::move(callback));
}

void RemoteCdmContext::GetHwKeyDataInternal(
    std::unique_ptr<media::DecryptConfig> decrypt_config,
    const std::vector<uint8_t>& hw_identifier,
    GetHwKeyDataCB callback) {
  // This can get called from decoder threads, so we may need to repost the
  // task.
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RemoteCdmContext::GetHwKeyDataInternal,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(decrypt_config), hw_identifier,
                                  std::move(callback)));
    return;
  }
  stable_cdm_context_->GetHwKeyData(std::move(decrypt_config), hw_identifier,
                                    std::move(callback));
}

void RemoteCdmContext::GetHwConfigData(GetHwConfigDataCB callback) {
  stable_cdm_context_->GetHwConfigData(std::move(callback));
}

void RemoteCdmContext::GetScreenResolutions(GetScreenResolutionsCB callback) {
  stable_cdm_context_->GetScreenResolutions(std::move(callback));
}

std::unique_ptr<media::CdmContextRef> RemoteCdmContext::GetCdmContextRef() {
  return std::make_unique<RemoteCdmContextRef>(base::WrapRefCounted(this));
}

bool RemoteCdmContext::UsingArcCdm() const {
  return false;
}

bool RemoteCdmContext::IsRemoteCdm() const {
  return true;
}

void RemoteCdmContext::EventCallback(media::CdmContext::Event event) {
  event_callbacks_.Notify(std::move(event));
}

void RemoteCdmContext::DeleteOnCorrectThread() const {
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    // When DeleteSoon returns false, |this| will be leaked, which is okay.
    mojo_task_runner_->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

// static
void RemoteCdmContextTraits::Destruct(
    const RemoteCdmContext* remote_cdm_context) {
  remote_cdm_context->DeleteOnCorrectThread();
}

}  // namespace chromeos
