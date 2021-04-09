// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include "base/memory/singleton.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

const int ClientSidePhishingModel::kInitialClientModelFetchDelayMs = 10000;

using base::AutoLock;

struct ClientSidePhishingModelSingletonTrait
    : public base::DefaultSingletonTraits<ClientSidePhishingModel> {
  static ClientSidePhishingModel* New() {
    ClientSidePhishingModel* instance = new ClientSidePhishingModel();
    return instance;
  }
};

// --- ClientSidePhishingModel methods ---

// static
ClientSidePhishingModel* ClientSidePhishingModel::GetInstance() {
  return base::Singleton<ClientSidePhishingModel,
                         ClientSidePhishingModelSingletonTrait>::get();
}

ClientSidePhishingModel::ClientSidePhishingModel() = default;

ClientSidePhishingModel::~ClientSidePhishingModel() {
  AutoLock lock(lock_);  // DCHECK fail if the lock is held.
}

base::CallbackListSubscription ClientSidePhishingModel::RegisterCallback(
    base::RepeatingCallback<void()> callback) {
  AutoLock lock(lock_);
  return callbacks_.Add(std::move(callback));
}

void ClientSidePhishingModel::Start(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  AutoLock lock(lock_);
  model_loader_ = std::make_unique<ModelLoader>(
      base::BindRepeating(&ClientSidePhishingModel::ModelUpdatedCallback,
                          base::Unretained(this)),
      url_loader_factory,
      /*extended_reporting=*/false);

  // Refresh the models when the service is enabled.  This can happen when
  // either of the preferences are toggled, or early during startup if
  // safe browsing is already enabled. In a lot of cases the model will be
  // in the cache so it  won't actually be fetched from the network.
  // We delay the first model fetches to avoid slowing down browser startup.
  model_loader_->ScheduleFetch(kInitialClientModelFetchDelayMs);
}

void ClientSidePhishingModel::Stop() {
  AutoLock lock(lock_);
  if (model_loader_) {
    model_loader_->CancelFetcher();
  }
  model_loader_ = nullptr;
}

bool ClientSidePhishingModel::IsEnabled() const {
  return model_loader_.get();
}

std::string ClientSidePhishingModel::GetModelStr() const {
  if (!overridden_model_str_.empty())
    return overridden_model_str_;
  return model_loader_ ? model_loader_->model_str() : "";
}

std::string ClientSidePhishingModel::GetModelName() const {
  return model_loader_ ? model_loader_->name() : "";
}

ModelLoader::ClientModelStatus ClientSidePhishingModel::GetLastModelStatus()
    const {
  return model_loader_ ? model_loader_->last_client_model_status()
                       : ModelLoader::MODEL_NEVER_FETCHED;
}

void ClientSidePhishingModel::SetModelStrForTesting(
    const std::string& model_str) {
  AutoLock lock(lock_);
  overridden_model_str_ = model_str;
}

void ClientSidePhishingModel::ModelUpdatedCallback() {
  AutoLock lock(lock_);
  callbacks_.Notify();
}

}  // namespace safe_browsing
