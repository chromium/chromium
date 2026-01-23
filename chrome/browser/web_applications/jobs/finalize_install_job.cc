// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/finalize_install_job.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

FinalizeInstallJob::FinalizeInstallJob(
    Profile& profile,
    WebAppProvider& provider,
    WebAppInstallFinalizer& finalizer,
    const WebAppInstallInfo& web_app_info,
    const WebAppInstallFinalizer::FinalizeOptions& options)
    : profile_(profile),
      provider_(provider),
      finalizer_(finalizer),
      web_app_info_(web_app_info.Clone()),
      options_(options) {}

FinalizeInstallJob::~FinalizeInstallJob() = default;

void FinalizeInstallJob::Start(
    WebAppInstallFinalizer::InstallFinalizedCallback callback) {
  callback_ = std::move(callback);
  webapps::ManifestId manifest_id = web_app_info_.manifest_id();

  // parent_app_manifest_id can only exist if installing as a sub-app.
  CHECK((options_.install_surface == webapps::WebappInstallSource::SUB_APP &&
         web_app_info_.parent_app_manifest_id.has_value()) ||
        (options_.install_surface != webapps::WebappInstallSource::SUB_APP &&
         !web_app_info_.parent_app_manifest_id.has_value()));

  bool needs_scope_validation =
      !web_app_info_.scope_extensions.empty() &&
      !web_app_info_.validated_scope_extensions.has_value();
  bool needs_migration_validation =
      base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi) &&
      !web_app_info_.migration_sources.empty();

  if (options_.skip_origin_association_validation ||
      (!needs_scope_validation && !needs_migration_validation)) {
    OnOriginAssociationValidated(OriginAssociations());
    return;
  }

  OriginAssociations origin_associations;
  if (needs_scope_validation) {
    origin_associations.scope_extensions = web_app_info_.scope_extensions;
  }
  if (needs_migration_validation) {
    origin_associations.migration_sources = web_app_info_.migration_sources;
  }
  provider_->origin_association_manager().GetWebAppOriginAssociations(
      manifest_id, std::move(origin_associations),
      base::BindOnce(&FinalizeInstallJob::OnOriginAssociationValidated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FinalizeInstallJob::OnOriginAssociationValidated(
    OriginAssociations validated_origin_associations) {
  // TODO:(crbug.com/259703817) OnOriginAssociationValidated should be moved
  // over here for complete refactor. Call the finalizer's
  // OnOriginAssociationValidated for now.
  finalizer_->OnOriginAssociationValidated(
      std::move(web_app_info_), std::move(options_), std::move(callback_),
      std::move(validated_origin_associations));
}

}  // namespace web_app
