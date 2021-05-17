// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

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

bool ClientSidePhishingModel::IsEnabled() const {
  return !model_str_.empty() || visual_tflite_model_.IsValid();
}

std::string ClientSidePhishingModel::GetModelStr() const {
  return model_str_;
}

const base::File& ClientSidePhishingModel::GetVisualTfLiteModel() const {
  return visual_tflite_model_;
}

void ClientSidePhishingModel::PopulateFromDynamicUpdate(
    const std::string& model_str,
    base::File visual_tflite_model) {
  AutoLock lock(lock_);

  bool proto_valid = false;
  if (!model_str.empty()) {
    ClientSideModel model_proto;
    proto_valid = model_proto.ParseFromString(model_str);
    base::UmaHistogramBoolean("SBClientPhishing.ModelDynamicUpdateSuccess",
                              proto_valid);

    if (proto_valid) {
      // At time of writing, versions go up to 25. We set a max version of 100
      // to give some room.
      const int kMaxVersion = 100;
      base::UmaHistogramExactLinear(
          "SBClientPhishing.ModelDynamicUpdateVersion", model_proto.version(),
          kMaxVersion + 1);
      model_str_ = model_str;
    }
  }

  bool tflite_valid = visual_tflite_model.IsValid();
  if (tflite_valid) {
    visual_tflite_model_ = std::move(visual_tflite_model);
  }

  if (proto_valid || tflite_valid) {
    // Unretained is safe because this is a singleton.
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                  base::Unretained(this)));
  }
}

void ClientSidePhishingModel::NotifyCallbacksOnUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.Notify();
}

void ClientSidePhishingModel::SetModelStrForTesting(
    const std::string& model_str) {
  AutoLock lock(lock_);
  model_str_ = model_str;
}

void ClientSidePhishingModel::SetVisualTfLiteModelForTesting(base::File file) {
  AutoLock lock(lock_);
  visual_tflite_model_ = std::move(file);
}

}  // namespace safe_browsing
