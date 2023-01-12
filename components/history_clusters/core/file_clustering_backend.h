// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FILE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FILE_CLUSTERING_BACKEND_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/history_clusters/core/clustering_backend.h"

namespace history_clusters {

namespace switches {

extern const char kClustersOverrideFile[];

}  // namespace switches

// A clustering backend that returns the clusters provided by a file specified
// by the command line.
class FileClusteringBackend : public ClusteringBackend {
 public:
  // Creates a FileClusteringBackend for overriding clusters if enabled
  // by command line switch.
  static std::unique_ptr<FileClusteringBackend> CreateIfEnabled();
  ~FileClusteringBackend() override;

  // ClusteringBackend:
  // `unused_requires_ui_and_triggerability` as this function just sends back
  // the clusters as is from the file.
  void GetClusters(ClusteringRequestSource clustering_request_source,
                   ClustersCallback callback,
                   std::vector<history::AnnotatedVisit> visits,
                   bool unused_requires_ui_and_triggerability) override;
  void GetClustersForUI(ClustersCallback callback,
                        std::vector<history::Cluster> clusters) override;
  void GetClusterTriggerability(
      ClustersCallback callback,
      std::vector<history::Cluster> clusters) override;

 private:
  // Private so that it does not incidentally get called if the command line is
  // invalid.
  FileClusteringBackend();

  // The background task runner that processes the file passes in the command
  // line and does the heavy lifting for responding to cluster requests.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FILE_CLUSTERING_BACKEND_H_
