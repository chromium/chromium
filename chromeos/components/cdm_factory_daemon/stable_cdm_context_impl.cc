// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/stable_cdm_context_impl.h"

#include "base/callback.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"

namespace chromeos {

StableCdmContextImpl::StableCdmContextImpl(media::CdmContext* cdm_context)
    : cdm_context_(cdm_context) {
  DCHECK(cdm_context_);
  DCHECK(cdm_context_->GetChromeOsCdmContext());
  cdm_context_ref_ = cdm_context_->GetChromeOsCdmContext()->GetCdmContextRef();
}

StableCdmContextImpl::~StableCdmContextImpl() {}

void StableCdmContextImpl::GetHwKeyData(
    std::unique_ptr<media::DecryptConfig> decrypt_config,
    const std::vector<uint8_t>& hw_identifier,
    GetHwKeyDataCallback callback) {
  cdm_context_->GetChromeOsCdmContext()->GetHwKeyData(
      decrypt_config.get(), hw_identifier, std::move(callback));
}

void StableCdmContextImpl::RegisterEventCallback(
    mojo::PendingRemote<media::stable::mojom::CdmContextEventCallback>
        callback) {
  remote_event_callbacks_.Add(std::move(callback));
  if (!callback_registration_) {
    callback_registration_ = cdm_context_->RegisterEventCB(
        base::BindRepeating(&StableCdmContextImpl::CdmEventCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void StableCdmContextImpl::GetHwConfigData(GetHwConfigDataCallback callback) {
  ChromeOsCdmFactory::GetHwConfigData(std::move(callback));
}

void StableCdmContextImpl::GetScreenResolutions(
    GetScreenResolutionsCallback callback) {
  ChromeOsCdmFactory::GetScreenResolutions(std::move(callback));
}

void StableCdmContextImpl::CdmEventCallback(media::CdmContext::Event event) {
  for (auto& cb : remote_event_callbacks_)
    cb->EventCallback(event);
}

}  // namespace chromeos
