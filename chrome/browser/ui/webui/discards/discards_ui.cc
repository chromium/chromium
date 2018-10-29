// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/discards_ui.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"
#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

mojom::LifecycleUnitDiscardReason GetDiscardReason(bool urgent) {
  return urgent ? mojom::LifecycleUnitDiscardReason::URGENT
                : mojom::LifecycleUnitDiscardReason::PROACTIVE;
}

mojom::LifecycleUnitVisibility GetLifecycleUnitVisibility(
    content::Visibility visibility) {
  switch (visibility) {
    case content::Visibility::HIDDEN:
      return mojom::LifecycleUnitVisibility::HIDDEN;
    case content::Visibility::OCCLUDED:
      return mojom::LifecycleUnitVisibility::OCCLUDED;
    case content::Visibility::VISIBLE:
      return mojom::LifecycleUnitVisibility::VISIBLE;
  }
#if defined(COMPILER_MSVC)
  NOTREACHED();
  return mojom::LifecycleUnitVisibility::VISIBLE;
#endif
}

resource_coordinator::LifecycleUnit* GetLifecycleUnitById(int32_t id) {
  for (resource_coordinator::LifecycleUnit* lifecycle_unit :
       g_browser_process->GetTabManager()->GetSortedLifecycleUnits()) {
    if (lifecycle_unit->GetID() == id)
      return lifecycle_unit;
  }
  return nullptr;
}

double GetSiteEngagementScore(content::WebContents* contents) {
  // Get the active navigation entry. Restored tabs should always have one.
  auto& controller = contents->GetController();
  const int current_entry_index = controller.GetCurrentEntryIndex();

  // A WebContents which hasn't navigated yet does not have a NavigationEntry.
  if (current_entry_index == -1)
    return 0;

  auto* nav_entry = controller.GetEntryAtIndex(current_entry_index);
  DCHECK(nav_entry);

  auto* engagement_svc = SiteEngagementService::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  return engagement_svc->GetDetails(nav_entry->GetURL()).total_score;
}

mojom::SiteCharacteristicsFeaturePtr ConvertFeatureFromProto(
    const SiteCharacteristicsFeatureProto& proto) {
  mojom::SiteCharacteristicsFeaturePtr feature =
      mojom::SiteCharacteristicsFeature::New();

  if (proto.has_observation_duration()) {
    feature->observation_duration = proto.observation_duration();
  } else {
    feature->observation_duration = 0;
  }

  if (proto.has_use_timestamp()) {
    feature->use_timestamp = proto.use_timestamp();
  } else {
    feature->use_timestamp = 0;
  }

  return feature;
}

mojom::SiteCharacteristicsDatabaseEntryPtr ConvertEntryFromProto(
    SiteCharacteristicsProto* proto) {
  mojom::SiteCharacteristicsDatabaseValuePtr value =
      mojom::SiteCharacteristicsDatabaseValue::New();

  if (proto->has_last_loaded()) {
    value->last_loaded = proto->last_loaded();
  } else {
    value->last_loaded = 0;
  }
  value->updates_favicon_in_background =
      ConvertFeatureFromProto(proto->updates_favicon_in_background());
  value->updates_title_in_background =
      ConvertFeatureFromProto(proto->updates_title_in_background());
  value->uses_audio_in_background =
      ConvertFeatureFromProto(proto->uses_audio_in_background());
  value->uses_notifications_in_background =
      ConvertFeatureFromProto(proto->uses_notifications_in_background());

  if (proto->has_load_time_estimates()) {
    const auto& load_time_estimates_proto = proto->load_time_estimates();
    DCHECK(load_time_estimates_proto.has_avg_cpu_usage_us());
    DCHECK(load_time_estimates_proto.has_avg_footprint_kb());

    mojom::SiteCharacteristicsPerformanceMeasurementPtr load_time_estimates =
        mojom::SiteCharacteristicsPerformanceMeasurement::New();
    if (load_time_estimates_proto.has_avg_cpu_usage_us()) {
      load_time_estimates->avg_cpu_usage_us =
          load_time_estimates_proto.avg_cpu_usage_us();
    }
    if (load_time_estimates_proto.has_avg_footprint_kb()) {
      load_time_estimates->avg_footprint_kb =
          load_time_estimates_proto.avg_footprint_kb();
    }
    if (load_time_estimates_proto.has_avg_load_duration_us()) {
      load_time_estimates->avg_load_duration_us =
          load_time_estimates_proto.avg_load_duration_us();
    }

    value->load_time_estimates = std::move(load_time_estimates);
  }

  mojom::SiteCharacteristicsDatabaseEntryPtr entry =
      mojom::SiteCharacteristicsDatabaseEntry::New();
  entry->value = std::move(value);
  return entry;
}

class DiscardsDetailsProviderImpl : public mojom::DiscardsDetailsProvider {
 public:
  // This instance is deleted when the supplied pipe is destroyed.
  DiscardsDetailsProviderImpl(
      resource_coordinator::LocalSiteCharacteristicsDataStoreInspector*
          data_store_inspector,
      mojo::InterfaceRequest<mojom::DiscardsDetailsProvider> request)
      : data_store_inspector_(data_store_inspector),
        binding_(this, std::move(request)) {}

  ~DiscardsDetailsProviderImpl() override {}

  // mojom::DiscardsDetailsProvider overrides:
  void GetTabDiscardsInfo(GetTabDiscardsInfoCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    const resource_coordinator::LifecycleUnitVector lifecycle_units =
        tab_manager->GetSortedLifecycleUnits();

    std::vector<mojom::TabDiscardsInfoPtr> infos;
    infos.reserve(lifecycle_units.size());

    const base::TimeTicks now = resource_coordinator::NowTicks();

    // Convert the LifecycleUnits to a vector of TabDiscardsInfos.
    size_t rank = 1;
    for (auto* lifecycle_unit : lifecycle_units) {
      mojom::TabDiscardsInfoPtr info(mojom::TabDiscardsInfo::New());

      resource_coordinator::TabLifecycleUnitExternal*
          tab_lifecycle_unit_external =
              lifecycle_unit->AsTabLifecycleUnitExternal();
      content::WebContents* contents =
          tab_lifecycle_unit_external->GetWebContents();

      info->tab_url = contents->GetLastCommittedURL().spec();
      info->title = base::UTF16ToUTF8(lifecycle_unit->GetTitle());
      info->visibility =
          GetLifecycleUnitVisibility(lifecycle_unit->GetVisibility());
      info->loading_state = lifecycle_unit->GetLoadingState();
      info->state = lifecycle_unit->GetState();
      resource_coordinator::DecisionDetails freeze_details;
      info->can_freeze = lifecycle_unit->CanFreeze(&freeze_details);
      info->cannot_freeze_reasons = freeze_details.GetFailureReasonStrings();
      resource_coordinator::DecisionDetails discard_details;
      info->can_discard = lifecycle_unit->CanDiscard(
          mojom::LifecycleUnitDiscardReason::PROACTIVE, &discard_details);
      info->cannot_discard_reasons = discard_details.GetFailureReasonStrings();
      info->discard_count = lifecycle_unit->GetDiscardCount();
      // This is only valid if the state is PENDING_DISCARD or DISCARD, but the
      // javascript code takes care of that.
      info->discard_reason = lifecycle_unit->GetDiscardReason();
      info->utility_rank = rank++;
      const base::TimeTicks last_focused_time =
          lifecycle_unit->GetLastFocusedTime();
      const base::TimeDelta elapsed =
          (last_focused_time == base::TimeTicks::Max())
              ? base::TimeDelta()
              : (now - last_focused_time);
      info->last_active_seconds = static_cast<int32_t>(elapsed.InSeconds());
      info->is_auto_discardable =
          tab_lifecycle_unit_external->IsAutoDiscardable();
      info->id = lifecycle_unit->GetID();
      base::Optional<float> reactivation_score =
          resource_coordinator::TabActivityWatcher::GetInstance()
              ->CalculateReactivationScore(contents);
      info->has_reactivation_score = reactivation_score.has_value();
      if (info->has_reactivation_score)
        info->reactivation_score = reactivation_score.value();
      info->site_engagement_score = GetSiteEngagementScore(contents);
      // TODO(crbug.com/876340): The focus is used to compute the page lifecycle
      // state. This should be replaced with the actual page lifecycle state
      // information from Blink, but this depends on implementing the passive
      // state and plumbing it to the browser.
      info->has_focus = lifecycle_unit->GetLastFocusedTime().is_max();

      infos.push_back(std::move(info));
    }

    std::move(callback).Run(std::move(infos));
  }
  void GetSiteCharacteristicsDatabase(
      const std::vector<std::string>& explicitly_requested_origins,
      GetSiteCharacteristicsDatabaseCallback callback) override;
  void GetSiteCharacteristicsDatabaseSize(
      GetSiteCharacteristicsDatabaseSizeCallback callback) override;

  void SetAutoDiscardable(int32_t id,
                          bool is_auto_discardable,
                          SetAutoDiscardableCallback callback) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit) {
      auto* tab_lifecycle_unit_external =
          lifecycle_unit->AsTabLifecycleUnitExternal();
      if (tab_lifecycle_unit_external)
        tab_lifecycle_unit_external->SetAutoDiscardable(is_auto_discardable);
    }
    std::move(callback).Run();
  }

  void DiscardById(int32_t id,
                   bool urgent,
                   DiscardByIdCallback callback) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit)
      lifecycle_unit->Discard(GetDiscardReason(urgent));
    std::move(callback).Run();
  }

  void FreezeById(int32_t id) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit)
      lifecycle_unit->Freeze();
  }

  void LoadById(int32_t id) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit)
      lifecycle_unit->Load();
  }

  void Discard(bool urgent, DiscardCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->DiscardTab(GetDiscardReason(urgent));
    std::move(callback).Run();
  }

 private:
  using LocalSiteCharacteristicsDataStoreInspector =
      resource_coordinator::LocalSiteCharacteristicsDataStoreInspector;
  using SiteCharacteristicsDataReader =
      resource_coordinator::SiteCharacteristicsDataReader;
  using SiteCharacteristicsDataStore =
      resource_coordinator::SiteCharacteristicsDataStore;
  using OriginToReaderMap =
      base::flat_map<std::string,
                     std::unique_ptr<SiteCharacteristicsDataReader>>;

  // This map pins requested readers and their associated data in memory until
  // after the next read finishes. This is necessary to allow the database reads
  // to go through and populate the requested entries.
  OriginToReaderMap requested_origins_;

  LocalSiteCharacteristicsDataStoreInspector* data_store_inspector_;
  mojo::Binding<mojom::DiscardsDetailsProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(DiscardsDetailsProviderImpl);
};

void DiscardsDetailsProviderImpl::GetSiteCharacteristicsDatabase(
    const std::vector<std::string>& explicitly_requested_origins,
    GetSiteCharacteristicsDatabaseCallback callback) {
  if (!data_store_inspector_) {
    // Early return with a nullptr if there's no inspector.
    std::move(callback).Run(nullptr);
    return;
  }

  // Move all previously explicitly requested origins to this local map.
  // Move any currently requested origins over to the member variable, or
  // populate them if they weren't previously requested.
  // The difference will remain in this map and go out of scope at the end of
  // this function.
  OriginToReaderMap prev_requested_origins;
  prev_requested_origins.swap(requested_origins_);
  SiteCharacteristicsDataStore* data_store =
      data_store_inspector_->GetDataStore();
  DCHECK(data_store);
  for (const std::string& origin : explicitly_requested_origins) {
    auto it = prev_requested_origins.find(origin);
    if (it == prev_requested_origins.end()) {
      GURL url(origin);
      requested_origins_[origin] =
          data_store->GetReaderForOrigin(url::Origin::Create(url));
    } else {
      requested_origins_[origin] = std::move(it->second);
      prev_requested_origins.erase(it);
    }
  }

  mojom::SiteCharacteristicsDatabasePtr result =
      mojom::SiteCharacteristicsDatabase::New();
  std::vector<url::Origin> in_memory_origins =
      data_store_inspector_->GetAllInMemoryOrigins();
  for (const url::Origin& origin : in_memory_origins) {
    // Get the data for this origin and convert it from proto to the
    // corresponding mojo structure.
    std::unique_ptr<SiteCharacteristicsProto> proto;
    bool is_dirty = false;
    if (data_store_inspector_->GetDataForOrigin(origin, &is_dirty, &proto)) {
      auto entry = ConvertEntryFromProto(proto.get());
      entry->origin = origin.Serialize();
      entry->is_dirty = is_dirty;
      result->db_rows.push_back(std::move(entry));
    }
  }

  // Return the result.
  std::move(callback).Run(std::move(result));
}

void DiscardsDetailsProviderImpl::GetSiteCharacteristicsDatabaseSize(
    GetSiteCharacteristicsDatabaseSizeCallback callback) {
  if (!data_store_inspector_) {
    // Early return with a nullptr if there's no inspector.
    std::move(callback).Run(nullptr);
    return;
  }

  // Adapt the inspector callback to the mojom callback with this lambda.
  auto inspector_callback = base::BindOnce(
      [](GetSiteCharacteristicsDatabaseSizeCallback callback,
         base::Optional<int64_t> num_rows,
         base::Optional<int64_t> on_disk_size_kb) {
        mojom::SiteCharacteristicsDatabaseSizePtr result =
            mojom::SiteCharacteristicsDatabaseSize::New();
        result->num_rows = num_rows.has_value() ? num_rows.value() : -1;
        result->on_disk_size_kb =
            on_disk_size_kb.has_value() ? on_disk_size_kb.value() : -1;

        std::move(callback).Run(std::move(result));
      },
      std::move(callback));

  data_store_inspector_->GetDatabaseSize(std::move(inspector_callback));
}

}  // namespace

DiscardsUI::DiscardsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIDiscardsHost));

  source->AddResourcePath("discards.js", IDR_DISCARDS_JS);

  source->AddResourcePath("discards_main.html",
                          IDR_DISCARDS_DISCARDS_MAIN_HTML);
  source->AddResourcePath("discards_main.js", IDR_DISCARDS_DISCARDS_MAIN_JS);

  source->AddResourcePath("database_tab.html", IDR_DISCARDS_DATABASE_TAB_HTML);
  source->AddResourcePath("database_tab.js", IDR_DISCARDS_DATABASE_TAB_JS);
  source->AddResourcePath("discards_tab.html", IDR_DISCARDS_DISCARDS_TAB_HTML);
  source->AddResourcePath("discards_tab.js", IDR_DISCARDS_DISCARDS_TAB_JS);
  source->AddResourcePath("sorted_table_behavior.html",
                          IDR_DISCARDS_SORTED_TABLE_BEHAVIOR_HTML);
  source->AddResourcePath("sorted_table_behavior.js",
                          IDR_DISCARDS_SORTED_TABLE_BEHAVIOR_JS);

  // Full paths (relative to src) are important for Mojom generated files.
  source->AddResourcePath("chrome/browser/ui/webui/discards/discards.mojom.js",
                          IDR_DISCARDS_MOJO_JS);
  source->AddResourcePath(
      "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.js",
      IDR_DISCARDS_LIFECYCLE_UNIT_STATE_MOJO_JS);
  source->AddResourcePath("mojom/webui_graph_dump.mojom.js",
                          IDR_DISCARDS_WEBUI_GRAPH_DUMP_MOJO_JS);

  // Add the mojo base dependency for the WebUI Graph Dump.
  source->AddResourcePath("mojo/public/mojom/base/process_id.mojom.js",
                          IDR_DISCARDS_MOJO_PUBLIC_BASE_PROCESS_ID_MOJOM_JS);

  source->SetDefaultResource(IDR_DISCARDS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source.release());

  AddHandlerToRegistry(base::BindRepeating(
      &DiscardsUI::BindDiscardsDetailsProvider, base::Unretained(this)));
  AddHandlerToRegistry(base::BindRepeating(
      &DiscardsUI::BindWebUIGraphDumpProvider, base::Unretained(this)));

  data_store_inspector_ = resource_coordinator::
      LocalSiteCharacteristicsDataStoreInspector::GetForProfile(profile);
}

DiscardsUI::~DiscardsUI() {}

void DiscardsUI::BindDiscardsDetailsProvider(
    mojom::DiscardsDetailsProviderRequest request) {
  ui_handler_ = std::make_unique<DiscardsDetailsProviderImpl>(
      data_store_inspector_, std::move(request));
}

void DiscardsUI::BindWebUIGraphDumpProvider(
    resource_coordinator::mojom::WebUIGraphDumpRequest request) {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();

  if (connector) {
    // Forward the interface request directly to the service.
    connector->BindInterface(resource_coordinator::mojom::kServiceName,
                             std::move(request));
  }
}
