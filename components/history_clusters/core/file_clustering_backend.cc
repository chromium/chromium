// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/file_clustering_backend.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history_clusters {

const char kClustersOverrideFile[] = "history-clusters-cluster-override-file";

namespace {

// Gets the file path that contains the clusters to use. Returns nullopt if the
// switch isn't on the command line or no file path is specified.
//
// Note that this does not validate that a valid path is specified, as that
// requires opening the file.
absl::optional<base::FilePath> GetClustersOverrideFilePath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kClustersOverrideFile)) {
    return absl::nullopt;
  }
  base::FilePath file_path =
      command_line->GetSwitchValuePath(kClustersOverrideFile);
  return file_path.empty() ? absl::nullopt : absl::make_optional(file_path);
}

// Reads the override file and builds the clusters from the file contents.
std::vector<history::Cluster> GetClustersFromFile() {
  absl::optional<base::FilePath> file_path = GetClustersOverrideFilePath();
  DCHECK(file_path);
  if (!file_path) {
    return {};
  }

  std::string file_contents;
  if (!base::ReadFileToString(*file_path, &file_contents)) {
    LOG(ERROR) << "Failed to read contents of clusters override file";
    return {};
  }

  absl::optional<base::Value> json_value =
      base::JSONReader::Read(file_contents);
  if (!json_value) {
    LOG(ERROR) << "Clusters override file is not valid JSON";
    return {};
  }

  // Parse the JSON.
  const base::Value* json_clusters = json_value->FindKey("clusters");
  if (!json_clusters || !json_clusters->is_list()) {
    return {};
  }
  std::vector<history::Cluster> clusters;
  clusters.reserve(json_clusters->GetList().size());

  for (const auto& json_cluster : json_clusters->GetList()) {
    const auto& json_cluster_dict = json_cluster.GetDict();

    history::Cluster cluster;

    // Get the visits associated with the cluster.
    const base::Value::List* visits = json_cluster_dict.FindList("visits");
    if (!visits) {
      continue;
    }
    for (const auto& json_visit : *visits) {
      const auto& json_visit_dict = json_visit.GetDict();

      absl::optional<double> score = json_visit_dict.FindDouble("score");
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
      cluster.visits.push_back(cluster_visit);
    }

    // Get the label.
    const std::string* label = json_cluster_dict.FindString("label");
    if (label) {
      cluster.label = base::UTF8ToUTF16(*label);
    }

    // Get whether it should be shown on prominent UI surfaces.
    absl::optional<bool> should_show_on_prominent_ui_surfaces =
        json_cluster_dict.FindBool("shouldShowOnProminentUiSurfaces");
    if (should_show_on_prominent_ui_surfaces) {
      cluster.should_show_on_prominent_ui_surfaces =
          *should_show_on_prominent_ui_surfaces;
    }

    clusters.push_back(cluster);
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
    std::vector<history::AnnotatedVisit> visits) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      // As `background_task_runner_` is owned and created by `this`, `this` is
      // guaranteed to outlive `background_task_runer_` so using
      // base::Unretained here is safe.
      base::BindOnce(&FileClusteringBackend::GetClustersOnBackgroundThread,
                     base::Unretained(this), std::move(visits)),
      base::BindOnce(std::move(callback)));
}

std::vector<history::Cluster>
FileClusteringBackend::GetClustersOnBackgroundThread(
    std::vector<history::AnnotatedVisit> visits) {
  if (!clusters_from_command_line_) {
    // Read and parse the file for clusters if we haven't attempted to already.
    clusters_from_command_line_ = GetClustersFromFile();
  }
  DCHECK(clusters_from_command_line_);

  // Build a map from visit ID to visit to make it easier for lookup when we
  // generate the clusters.
  base::flat_map<history::VisitID, history::AnnotatedVisit>
      visit_id_to_visit_map;
  for (const auto& visit : visits) {
    visit_id_to_visit_map[visit.visit_row.visit_id] = visit;
  }

  std::vector<history::Cluster> clusters;
  for (const auto& cluster : *clusters_from_command_line_) {
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
      in_progress_cluster.visits.push_back(in_progress_cluster_visit);
    }

    // Only put a cluster in if it has at least one visit.
    if (!in_progress_cluster.visits.empty()) {
      clusters.push_back(in_progress_cluster);
    }
  }

  return clusters;
}

}  // namespace history_clusters