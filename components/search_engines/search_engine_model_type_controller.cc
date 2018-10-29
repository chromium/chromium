// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_model_type_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace browser_sync {

namespace {

// Similar to ForwardingModelTypeControllerDelegate, but allows owning a bridge
// and deferring OnSyncStarting() until TemplateURLService is loaded.
class ControllerDelegate
    : public syncer::ForwardingModelTypeControllerDelegate {
 public:
  // |bridge| and |template_url_service| must not be null.
  ControllerDelegate(std::unique_ptr<syncer::ModelTypeSyncBridge> bridge,
                     TemplateURLService* template_url_service)
      : ForwardingModelTypeControllerDelegate(
            bridge->change_processor()->GetControllerDelegate().get()),
        bridge_(std::move(bridge)),
        template_url_service_(template_url_service) {}

  ~ControllerDelegate() override = default;

  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback callback) override {
    DCHECK(!template_url_subscription_);

    // We force a load here to allow remote updates to be processed, without
    // waiting for TemplateURLService's lazy load.
    template_url_service_->Load();

    // If the service is loaded, continue normally, which means requesting the
    // processor to start.
    if (template_url_service_->loaded()) {
      ForwardingModelTypeControllerDelegate::OnSyncStarting(
          request, std::move(callback));
      return;
    }

    // Otherwise, wait until it becomes ready. Using base::Unretained() should
    // be safe here because the subscription itself will be destroyed together
    // with this object.
    template_url_subscription_ =
        template_url_service_->RegisterOnLoadedCallback(
            base::AdaptCallbackForRepeating(base::BindOnce(
                &ControllerDelegate::OnTemplateURLServiceLoaded,
                base::Unretained(this), request, std::move(callback))));
  }

 private:
  void OnTemplateURLServiceLoaded(
      const syncer::DataTypeActivationRequest& request,
      StartCallback callback) {
    template_url_subscription_.reset();
    DCHECK(template_url_service_->loaded());
    // Now that we're loaded, continue normally, which means requesting the
    // processor to start.
    ForwardingModelTypeControllerDelegate::OnSyncStarting(request,
                                                          std::move(callback));
  }

  const std::unique_ptr<syncer::ModelTypeSyncBridge> bridge_;
  TemplateURLService* const template_url_service_;
  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ControllerDelegate);
};

// Helper function to construct the various objects needed to run this datatype.
std::unique_ptr<ControllerDelegate> BuildDelegate(
    const base::RepeatingClosure& dump_stack,
    syncer::OnceModelTypeStoreFactory store_factory,
    TemplateURLService* template_url_service) {
  DCHECK(template_url_service);

  auto processor = std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
      syncer::SEARCH_ENGINES, dump_stack);

  auto bridge = std::make_unique<syncer::SyncableServiceBasedBridge>(
      syncer::SEARCH_ENGINES, std::move(store_factory), std::move(processor),
      template_url_service);

  return std::make_unique<ControllerDelegate>(std::move(bridge),
                                              template_url_service);
}

}  // namespace

SearchEngineModelTypeController::SearchEngineModelTypeController(
    const base::RepeatingClosure& dump_stack,
    syncer::OnceModelTypeStoreFactory store_factory,
    TemplateURLService* template_url_service)
    : ModelTypeController(syncer::SEARCH_ENGINES,
                          BuildDelegate(dump_stack,
                                        std::move(store_factory),
                                        template_url_service)) {}

SearchEngineModelTypeController::~SearchEngineModelTypeController() = default;

}  // namespace browser_sync
