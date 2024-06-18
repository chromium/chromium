// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/site_data_provider_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/persistence/site_data/site_data.pb.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "url/gurl.h"

namespace {

discards::mojom::SiteDataFeaturePtr ConvertFeatureFromProto(
    const SiteDataFeatureProto& proto) {
  discards::mojom::SiteDataFeaturePtr feature =
      discards::mojom::SiteDataFeature::New();

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

discards::mojom::SiteDataEntryPtr ConvertEntryFromProto(SiteDataProto* proto) {
  discards::mojom::SiteDataValuePtr value =
      discards::mojom::SiteDataValue::New();

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

  if (proto->has_load_time_estimates()) {
    const auto& load_time_estimates_proto = proto->load_time_estimates();
    DCHECK(load_time_estimates_proto.has_avg_cpu_usage_us());
    DCHECK(load_time_estimates_proto.has_avg_footprint_kb());

    discards::mojom::SiteDataPerformanceMeasurementPtr load_time_estimates =
        discards::mojom::SiteDataPerformanceMeasurement::New();
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

  discards::mojom::SiteDataEntryPtr entry =
      discards::mojom::SiteDataEntry::New();
  entry->value = std::move(value);
  return entry;
}

}  // namespace

SiteDataProviderImpl::SiteDataProviderImpl(const std::string& profile_id)
    : profile_id_(profile_id) {}

SiteDataProviderImpl::~SiteDataProviderImpl() = default;

// static
void SiteDataProviderImpl::CreateAndBind(
    mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver,
    const std::string& profile_id_,
    performance_manager::Graph* graph) {
  std::unique_ptr<SiteDataProviderImpl> site_data_provider =
      std::make_unique<SiteDataProviderImpl>(profile_id_);

  site_data_provider->Bind(std::move(receiver));
  graph->PassToGraph(std::move(site_data_provider));
}

void SiteDataProviderImpl::GetSiteDataArray(
    const std::vector<std::string>& explicitly_requested_origins,
    GetSiteDataArrayCallback callback) {
  performance_manager::SiteDataCacheInspector* inspector = nullptr;
  if (auto* factory =
          performance_manager::SiteDataCacheFactory::GetInstance()) {
    inspector = factory->GetInspectorForBrowserContext(profile_id_);
  }
  if (!inspector) {
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
  performance_manager::SiteDataCache* data_cache = inspector->GetDataCache();
  DCHECK(data_cache);

  for (const std::string& origin : explicitly_requested_origins) {
    auto it = prev_requested_origins.find(origin);
    if (it == prev_requested_origins.end()) {
      GURL url(origin);
      requested_origins_[origin] =
          data_cache->GetReaderForOrigin(url::Origin::Create(url));
    } else {
      requested_origins_[origin] = std::move(it->second);
      prev_requested_origins.erase(it);
    }
  }

  discards::mojom::SiteDataArrayPtr result =
      discards::mojom::SiteDataArray::New();
  std::vector<url::Origin> in_memory_origins =
      inspector->GetAllInMemoryOrigins();
  for (const url::Origin& origin : in_memory_origins) {
    // Get the data for this origin and convert it from proto to the
    // corresponding mojo structure.
    std::unique_ptr<SiteDataProto> proto;
    bool is_dirty = false;
    if (inspector->GetDataForOrigin(origin, &is_dirty, &proto)) {
      auto entry = ConvertEntryFromProto(proto.get());
      entry->origin = origin.Serialize();
      entry->is_dirty = is_dirty;
      result->db_rows.push_back(std::move(entry));
    }
  }

  // Return the result.
  std::move(callback).Run(std::move(result));
}

void SiteDataProviderImpl::GetSiteDataDatabaseSize(
    GetSiteDataDatabaseSizeCallback callback) {
  performance_manager::SiteDataCacheInspector* inspector = nullptr;
  if (auto* factory =
          performance_manager::SiteDataCacheFactory::GetInstance()) {
    inspector = factory->GetInspectorForBrowserContext(profile_id_);
  }
  if (!inspector) {
    // Early return with a nullptr if there's no inspector.
    std::move(callback).Run(nullptr);
    return;
  }

  // Adapt the inspector callback to the mojom callback with this lambda.
  auto inspector_callback = base::BindOnce(
      [](GetSiteDataDatabaseSizeCallback callback,
         std::optional<int64_t> num_rows,
         std::optional<int64_t> on_disk_size_kb) {
        discards::mojom::SiteDataDatabaseSizePtr result =
            discards::mojom::SiteDataDatabaseSize::New();
        result->num_rows = num_rows.has_value() ? num_rows.value() : -1;
        result->on_disk_size_kb =
            on_disk_size_kb.has_value() ? on_disk_size_kb.value() : -1;

        std::move(callback).Run(std::move(result));
      },
      std::move(callback));

  inspector->GetDataStoreSize(std::move(inspector_callback));
}

// static
void SiteDataProviderImpl::OnConnectionError(SiteDataProviderImpl* impl) {
  std::unique_ptr<performance_manager::GraphOwned> owned_impl =
      impl->GetOwningGraph()->TakeFromGraph(impl);
}

void SiteDataProviderImpl::Bind(
    mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SiteDataProviderImpl::OnConnectionError, base::Unretained(this)));
}
