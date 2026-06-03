// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/update_validated_origin_associations_command.h"

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
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
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
#include "net/base/network_change_notifier.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

UpdateValidatedOriginAssociationsCommand::
    UpdateValidatedOriginAssociationsCommand(
        const webapps::AppId& app_id,
        base::OnceCallback<void(UpdateValidatedOriginAssociationsResult)>
            callback)
    : WebAppCommand<AppLock, UpdateValidatedOriginAssociationsResult>(
          "UpdateValidatedOriginAssociationsResult",
          AppLockDescription(app_id),
          base::BindOnce([](UpdateValidatedOriginAssociationsResult result) {
            base::UmaHistogramEnumeration(
                "WebApp.ValidatedOriginAssociations.Updated", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          UpdateValidatedOriginAssociationsResult::kShutdown),
      app_id_(app_id) {}

UpdateValidatedOriginAssociationsCommand::
    ~UpdateValidatedOriginAssociationsCommand() = default;

void UpdateValidatedOriginAssociationsCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (net::NetworkChangeNotifier::IsOffline()) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            UpdateValidatedOriginAssociationsResult::kOffline);
    return;
  }

  const WebAppRegistrar& registrar = lock_->registrar();
  const WebApp* app =
      registrar.GetAppById(app_id_, WebAppFilter::InstalledInChrome());

  if (!app) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        UpdateValidatedOriginAssociationsResult::kWebAppNotInstalled);
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
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        UpdateValidatedOriginAssociationsResult::kThrottled);
    return;
  }

  if (*app->origin_association_last_validation_check_time() + base::Days(1) >
      lock_->clock().Now()) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        UpdateValidatedOriginAssociationsResult::kThrottled);
    return;
  }

  OriginAssociations origin_associations;
  origin_associations.scope_extensions = app->scope_extensions();
  origin_associations.migration_sources = app->unvalidated_migration_sources();

  lock_->origin_association_manager().GetWebAppOriginAssociations(
      app->manifest_id().value(), std::move(origin_associations),
      base::BindOnce(&UpdateValidatedOriginAssociationsCommand::
                         OnOriginAssociationValidated,
                     weak_factory_.GetWeakPtr()));
}

void UpdateValidatedOriginAssociationsCommand::OnOriginAssociationValidated(
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
      unvalidated_items_remain =
          !base::STLSetDifference<base::flat_set<ScopeExtensionInfo>>(
               app.scope_extensions(),
               validated_origin_associations.scope_extensions)
               .empty();

      scope_extensions_updated =
          validated_origin_associations.scope_extensions !=
          app.validated_scope_extensions();
      app.SetValidatedScopeExtensions(
          std::move(validated_origin_associations.scope_extensions));
    }

    {
      base::flat_set<MigrationSource> original_unvalidated(
          app.unvalidated_migration_sources());
      base::flat_set<MigrationSource> original_validated(
          app.validated_migration_sources());

      base::flat_set<MigrationSource> new_validated(
          validated_origin_associations.migration_sources);

      unvalidated_items_remain =
          unvalidated_items_remain ||
          !base::STLSetDifference<base::flat_set<MigrationSource>>(
               original_unvalidated, new_validated)
               .empty();

      // Check if any migration sources were added or removed.
      migration_sources_updated = original_validated != new_validated;

      app.SetValidatedMigrationSources(std::move(new_validated).extract());
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
        UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain);
  } else {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            UpdateValidatedOriginAssociationsResult::kSuccess);
  }
}

}  //  namespace web_app
