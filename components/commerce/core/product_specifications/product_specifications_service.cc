// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace commerce {

ProductSpecificationsService::ProductSpecificationsService(
    std::unique_ptr<ProductSpecificationsSyncBridge> bridge,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : bridge_(std::move(bridge)), backend_task_runner_(backend_task_runner) {}

ProductSpecificationsService::~ProductSpecificationsService() = default;

std::unique_ptr<syncer::ModelTypeControllerDelegate>
ProductSpecificationsService::CreateSyncControllerDelegate() {
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      backend_task_runner_,
      base::BindRepeating(
          &ProductSpecificationsService::GetSyncControllerDelegate,
          base::Unretained(this)));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ProductSpecificationsService::GetSyncControllerDelegate() {
  CHECK(bridge_);
  return bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace commerce
