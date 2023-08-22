// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/stable_cdm_context_impl.h"

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"

namespace chromeos {

StableCdmContextImpl::StableCdmContextImpl(media::CdmContext* cdm_context)
    : cdm_context_(cdm_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cdm_context_);
  DCHECK(cdm_context_->GetChromeOsCdmContext());
  cdm_context_ref_ = cdm_context_->GetChromeOsCdmContext()->GetCdmContextRef();
}

StableCdmContextImpl::~StableCdmContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StableCdmContextImpl::GetHwKeyData(
    std::unique_ptr<media::DecryptConfig> decrypt_config,
    const std::vector<uint8_t>& hw_identifier,
    GetHwKeyDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cdm_context_->GetChromeOsCdmContext()->GetHwKeyData(
      decrypt_config.get(), hw_identifier,
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::RegisterEventCallback(
    mojo::PendingRemote<media::stable::mojom::CdmContextEventCallback>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: we don't need to use base::BindPostTaskToCurrentDefault() for either
  // |callback| or the callback we pass to RegisterEventCB() because the
  // documentation for media::CdmContext::RegisterEventCB() says that "[t]he
  // registered callback will always be called on the thread where
  // RegisterEventCB() is called."
  remote_event_callbacks_.Add(std::move(callback));
  if (!callback_registration_) {
    callback_registration_ = cdm_context_->RegisterEventCB(
        base::BindRepeating(&StableCdmContextImpl::CdmEventCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void StableCdmContextImpl::GetHwConfigData(GetHwConfigDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeOsCdmFactory::GetHwConfigData(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::GetScreenResolutions(
    GetScreenResolutionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeOsCdmFactory::GetScreenResolutions(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::AllocateSecureBuffer(
    uint32_t size,
    AllocateSecureBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cdm_context_->GetChromeOsCdmContext()->AllocateSecureBuffer(
      size, std::move(callback));
}

void StableCdmContextImpl::CdmEventCallback(media::CdmContext::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& cb : remote_event_callbacks_)
    cb->EventCallback(event);
}

}  // namespace chromeos
