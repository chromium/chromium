// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/file_clustering_backend.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/history_clusters/core/filter_cluster_processor.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {

namespace switches {

const char kClustersOverrideFile[] = "history-clusters-cluster-override-file";

}  // namespace switches

namespace {

// Gets the file path that contains the clusters to use. Returns nullopt if the
// switch isn't on the command line or no file path is specified.
//
// Note that this does not validate that a valid path is specified, as that
// requires opening the file.
std::optional<base::FilePath> GetClustersOverrideFilePath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kClustersOverrideFile)) {
    return std::nullopt;
  }
  base::FilePath file_path =
      command_line->GetSwitchValuePath(switches::kClustersOverrideFile);
  return file_path.empty() ? std::nullopt : std::make_optional(file_path);
}

// Reads the override file and builds the clusters from the file contents.
std::vector<history::Cluster> GetClustersFromFile() {
  std::optional<base::FilePath> file_path = GetClustersOverrideFilePath();
  DCHECK(file_path);
  if (!file_path) {
    return {};
  }

  std::string file_contents;
  if (!base::ReadFileToString(*file_path, &file_contents)) {
    LOG(ERROR) << "Failed to read contents of clusters override file";
    return {};
  }

  std::optional<base::Value> json_value = base::JSONReader::Read(file_contents);
  if (!json_value) {
    LOG(ERROR) << "Clusters override file is not valid JSON";
    return {};
  }

  // Parse the JSON.
  const base::Value::List* json_clusters =
      json_value->GetDict().FindList("clusters");
  if (!json_clusters) {
    return {};
  }
  std::vector<history::Cluster> clusters;
  clusters.reserve(json_clusters->size());

  for (const auto& json_cluster : *json_clusters) {
    const auto& json_cluster_dict = json_cluster.GetDict();

    history::Cluster cluster;

    // Get the visits associated with the cluster.
    const base::Value::List* visits = json_cluster_dict.FindList("visits");
    if (!visits) {
      continue;
    }
    for (const auto& json_visit : *visits) {
      const auto& json_visit_dict = json_visit.GetDict();

      std::optional<double> score = json_visit_dict.FindDouble("score");
      if (!score) {
        continue;
      }

      // int64's are serialized as strings in JSON.
      const std::string* visit_id_string =
          json_visit_dict.FindString("visitId");
      if (!visit_id_string) {
        continue;
      }
      int64_t visit_id;
      if (!base::StringToInt64(*visit_id_string, &visit_id)) {
        continue;
      }

      history::ClusterVisit cluster_visit;
      cluster_visit.annotated_visit.visit_row.visit_id = visit_id;
      cluster_visit.score = static_cast<float>(*score);

      // Get duplicate visit IDs.
      const base::Value::List* duplicate_visit_ids =
          json_visit_dict.FindList("duplicateVisitIds");
      if (duplicate_visit_ids) {
        for (const auto& json_duplicate_visit_id : *duplicate_visit_ids) {
          int64_t duplicate_visit_id;
          if (base::StringToInt64(json_duplicate_visit_id.GetString(),
                                  &duplicate_visit_id)) {
            cluster_visit.duplicate_visits.push_back({duplicate_visit_id});
          }
        }
      }

      const std::string* image_url_string =
          json_visit_dict.FindString("imageUrl");
      if (image_url_string) {
        cluster_visit.image_url = GURL(*image_url_string);
      }
      cluster.visits.push_back(cluster_visit);
    }

    // Get the label.
    const std::string* label = json_cluster_dict.FindString("label");
    if (label) {
      cluster.label = base::UTF8ToUTF16(*label);
    }

    // Get whether it should be shown on prominent UI surfaces.
    std::optional<bool> should_show_on_prominent_ui_surfaces =
        json_cluster_dict.FindBool("shouldShowOnProminentUiSurfaces");
    if (should_show_on_prominent_ui_surfaces) {
      cluster.should_show_on_prominent_ui_surfaces =
          *should_show_on_prominent_ui_surfaces;
    }

    clusters.push_back(cluster);
  }

  return clusters;
}

std::vector<history::Cluster> GetClustersOnBackgroundThread(
    std::vector<history::AnnotatedVisit> visits) {
  std::vector<history::Cluster> clusters_from_command_line =
      GetClustersFromFile();

  // Build a map from visit ID to visit to make it easier for lookup when we
  // generate the clusters.
  base::flat_map<history::VisitID, history::AnnotatedVisit>
      visit_id_to_visit_map;
  for (const auto& visit : visits) {
    visit_id_to_visit_map[visit.visit_row.visit_id] = visit;
  }

  std::vector<history::Cluster> clusters;
  for (const auto& cluster : clusters_from_command_line) {
    history::Cluster in_progress_cluster = cluster;
    in_progress_cluster.visits.clear();

    // Filter clusters with only the visits that are requested rather than keep
    // them as is.
    for (const auto& cluster_visit : cluster.visits) {
      auto it = visit_id_to_visit_map.find(
          cluster_visit.annotated_visit.visit_row.visit_id);
      if (it == visit_id_to_visit_map.end()) {
        continue;
      }

      history::ClusterVisit in_progress_cluster_visit = cluster_visit;
      in_progress_cluster_visit.annotated_visit = it->second;
      in_progress_cluster_visit.normalized_url =
          it->second.content_annotations.search_normalized_url.is_empty()
              ? it->second.url_row.url()
              : it->second.content_annotations.search_normalized_url;
      in_progress_cluster_visit.url_for_display =
          ComputeURLForDisplay(in_progress_cluster_visit.normalized_url);

      // Fill in duplicate visits.
      in_progress_cluster_visit.duplicate_visits.clear();
      for (const auto& duplicate_visit : cluster_visit.duplicate_visits) {
        auto duplicate_visit_it =
            visit_id_to_visit_map.find(duplicate_visit.visit_id);
        if (duplicate_visit_it == visit_id_to_visit_map.end()) {
          continue;
        }

        const auto& full_duplicate_visit = duplicate_visit_it->second;
        in_progress_cluster_visit.duplicate_visits.push_back(
            {full_duplicate_visit.visit_row.visit_id,
             full_duplicate_visit.url_row.url(),
             full_duplicate_visit.visit_row.visit_time});
      }

      in_progress_cluster.visits.push_back(in_progress_cluster_visit);
    }

    // Only put a cluster in if it has at least one visit.
    if (!in_progress_cluster.visits.empty()) {
      clusters.push_back(in_progress_cluster);
    }
  }

  return clusters;
}

}  // namespace

FileClusteringBackend::FileClusteringBackend()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}
FileClusteringBackend::~FileClusteringBackend() = default;

// static
std::unique_ptr<FileClusteringBackend>
FileClusteringBackend::CreateIfEnabled() {
  return GetClustersOverrideFilePath()
             ? base::WrapUnique<FileClusteringBackend>(
                   new FileClusteringBackend)
             : nullptr;
}

void FileClusteringBackend::GetClusters(
    ClusteringRequestSource clustering_request_source,
    ClustersCallback callback,
    std::vector<history::AnnotatedVisit> visits,
    bool unused_requires_ui_and_triggerability) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetClustersOnBackgroundThread, std::move(visits)),
      base::BindOnce(std::move(callback)));
}

void FileClusteringBackend::GetClustersForUI(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    ClustersCallback callback,
    std::vector<history::Cluster> clusters) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileClusteringBackend::GetClustersForUIOnBackgroundThread,
                     clustering_request_source, std::move(filter_params),
                     std::move(clusters)),
      base::BindOnce(std::move(callback)));
}

std::vector<history::Cluster>
FileClusteringBackend::GetClustersForUIOnBackgroundThread(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    std::vector<history::Cluster> persisted_clusters) {
  // Return if no persisted_clusters.
  if (persisted_clusters.empty()) {
    return {};
  }

  // Construct map of all visit ids to visits in persisted clusters.
  std::vector<history::Cluster> clusters_from_command_line =
      GetClustersFromFile();
  base::flat_map<int64_t, history::ClusterVisit> persisted_visit_id_visit_map;
  for (const auto& cluster : persisted_clusters) {
    for (auto& visit : cluster.visits) {
      persisted_visit_id_visit_map.insert(
          {visit.annotated_visit.visit_row.visit_id, visit});
    }
  }

  // Patch visits from persistence onto clusters from command line.
  for (auto& cluster : clusters_from_command_line) {
    for (auto it = cluster.visits.begin(); it != cluster.visits.end(); ++it) {
      if (persisted_visit_id_visit_map.contains(
              it->annotated_visit.visit_row.visit_id)) {
        *it = persisted_visit_id_visit_map[it->annotated_visit.visit_row
                                               .visit_id];
        continue;
      }
      cluster.visits.erase(it);
    }
  }

  // Apply any filtering after we've patched clusters from file.
  auto filterer = std::make_unique<FilterClusterProcessor>(
      clustering_request_source, filter_params,
      /*engagement_score_provider_is_valid=*/true);
  filterer->ProcessClusters(&clusters_from_command_line);
  return clusters_from_command_line;
}

void FileClusteringBackend::GetClusterTriggerability(
    ClustersCallback callback,
    std::vector<history::Cluster> clusters) {
  std::move(callback).Run(std::move(clusters));
}

}  // namespace history_clusters
