// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/add_validated_origin_associations_command.h"

#include <algorithm>
#include <ranges>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/model/migration_source.h"
#include "chrome/browser/web_applications/scheduler/add_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/sync/base/time.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

AddValidatedOriginAssociationsCommand::AddValidatedOriginAssociationsCommand(
    const webapps::AppId& app_id,
    base::OnceCallback<void(AddValidatedOriginAssociationsResult)> callback)
    : WebAppCommand<AppLock, AddValidatedOriginAssociationsResult>(
          "WebApp.AddValidatedOriginAssociations",
          AppLockDescription(app_id),
          base::BindOnce([](AddValidatedOriginAssociationsResult result) {
            base::UmaHistogramEnumeration(
                "WebApp.AddValidatedOriginAssociations", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          AddValidatedOriginAssociationsResult::kShutdown),
      app_id_(app_id) {}

AddValidatedOriginAssociationsCommand::
    ~AddValidatedOriginAssociationsCommand() = default;

void AddValidatedOriginAssociationsCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  const WebAppRegistrar& registrar = lock_->registrar();
  const WebApp* app =
      registrar.GetAppById(app_id_, WebAppFilter::InstalledInChrome());

  if (!app) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        AddValidatedOriginAssociationsResult::kWebAppNotInstalled);
    return;
  }

  bool needs_scope_validation =
      !base::STLSetDifference<base::flat_set<ScopeExtensionInfo>>(
           app->scope_extensions(), app->validated_scope_extensions())
           .empty();

  bool needs_migration_validation =
      !base::STLSetDifference<base::flat_set<MigrationSource>>(
           base::flat_set<MigrationSource>(
               app->unvalidated_migration_sources()),
           base::flat_set<MigrationSource>(app->validated_migration_sources()))
           .empty();

  if (!needs_scope_validation && !needs_migration_validation) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            AddValidatedOriginAssociationsResult::kNotNeeded);
    return;
  }

  // If the last validation time isn't set, randomize it in the past to ensure
  // no network fetch spikes.
  if (!app->origin_association_last_validation_check_time().has_value()) {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp& app_to_update = CHECK_DEREF(update->UpdateApp(app_id_));
    base::TimeDelta delta =
        base::Seconds(base::RandIntInclusive(0, base::Days(1).InSeconds()));
    app_to_update.SetOriginAssociationLastValidationCheckTime(
        lock_->clock().Now() + delta + base::Days(1));
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            AddValidatedOriginAssociationsResult::kThrottled);
    return;
  }

  if (*app->origin_association_last_validation_check_time() + base::Days(1) >
      lock_->clock().Now()) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            AddValidatedOriginAssociationsResult::kThrottled);
    return;
  }

  OriginAssociations origin_associations;
  origin_associations.scope_extensions = app->scope_extensions();
  origin_associations.migration_sources = app->unvalidated_migration_sources();

  lock_->origin_association_manager().GetWebAppOriginAssociations(
      app->manifest_id(), std::move(origin_associations),
      base::BindOnce(
          &AddValidatedOriginAssociationsCommand::OnOriginAssociationValidated,
          weak_factory_.GetWeakPtr()));
}

void AddValidatedOriginAssociationsCommand::OnOriginAssociationValidated(
    OriginAssociations validated_origin_associations) {
  const base::Time now_time =
      syncer::ProtoTimeToTime(syncer::TimeToProtoTime(lock_->clock().Now()));

  bool unvalidated_items_remain = false;
  bool migration_sources_updated = false;
  bool scope_extensions_updated = false;
  {
    web_app::ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    web_app::WebApp& app = CHECK_DEREF(update->UpdateApp(app_id_));

    {
      // Gather union of current validated scope extensions and received.
      auto previously_validated_and_requested =
          base::STLSetIntersection<base::flat_set<ScopeExtensionInfo>>(
              app.validated_scope_extensions(), app.scope_extensions());

      auto final_validated =
          base::STLSetUnion<base::flat_set<ScopeExtensionInfo>>(
              previously_validated_and_requested,
              validated_origin_associations.scope_extensions);

      unvalidated_items_remain =
          !base::STLSetDifference<base::flat_set<ScopeExtensionInfo>>(
               app.scope_extensions(), final_validated)
               .empty();

      scope_extensions_updated =
          final_validated != app.validated_scope_extensions();
      app.SetValidatedScopeExtensions(std::move(final_validated));
    }

    {
      base::flat_set<MigrationSource> original_unvalidated(
          app.unvalidated_migration_sources());
      base::flat_set<MigrationSource> original_validated(
          app.validated_migration_sources());

      base::flat_set<MigrationSource> new_validated(
          validated_origin_associations.migration_sources);

      auto previously_validated_and_requested =
          base::STLSetIntersection<base::flat_set<MigrationSource>>(
              original_unvalidated, original_validated);

      auto final_validated = base::STLSetUnion<base::flat_set<MigrationSource>>(
          previously_validated_and_requested, new_validated);

      unvalidated_items_remain =
          unvalidated_items_remain ||
          !base::STLSetDifference<base::flat_set<MigrationSource>>(
               original_unvalidated, final_validated)
               .empty();

      // Check if any migration sources were added or removed.
      migration_sources_updated = original_validated != final_validated;

      app.SetValidatedMigrationSources(std::move(final_validated).extract());
    }
    app.SetOriginAssociationLastValidationCheckTime(now_time);
  }

  if (migration_sources_updated &&
      base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi)) {
    lock_->scheduler().ScheduleResolveWebAppPendingMigrationInfo(
        base::DoNothing());
  }

  if (scope_extensions_updated) {
    lock_->registrar().NotifyWebAppEffectiveScopeChanged(app_id_);
  }

  if (unvalidated_items_remain) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain);
  } else {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            AddValidatedOriginAssociationsResult::kSuccess);
  }
}

}  //  namespace web_app
