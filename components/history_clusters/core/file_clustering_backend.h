// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FILE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FILE_CLUSTERING_BACKEND_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/history_clusters/core/clustering_backend.h"

namespace history_clusters {

extern const char kClustersOverrideFile[];

// A clustering backend that returns the clusters provided by a file specified
// by the command line.
class FileClusteringBackend : public ClusteringBackend {
 public:
  // Creates a FileClusteringBackend for overriding clusters if enabled
  // by command line switch.
  static std::unique_ptr<FileClusteringBackend> CreateIfEnabled();
  ~FileClusteringBackend() override;

  // ClusteringBackend:
  void GetClusters(ClusteringRequestSource clustering_request_source,
                   ClustersCallback callback,
                   std::vector<history::AnnotatedVisit> visits) override;

 private:
  // Private so that it does not incidentally get called if the command line is
  // invalid.
  FileClusteringBackend();

  // Fulfills the request for the clusters for `visits` by returning
  // all non-empty clusters after `clusters_from_command_line_` is filtered by
  // `visits`. Will get the clusters from the file specified by the command line
  // if file has not attempted to be read yet.
  std::vector<history::Cluster> GetClustersOnBackgroundThread(
      std::vector<history::AnnotatedVisit> visits);

  // The background task runner that processes the file passes in the command
  // line and does the heavy lifting for responding to cluster requests.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The clusters passed via command line. Will be nullopt until first call to
  // `GetClusters`. Should only be accessed on the same sequence as
  // `background_task_runner_`.
  absl::optional<std::vector<history::Cluster>> clusters_from_command_line_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FILE_CLUSTERING_BACKEND_H_
