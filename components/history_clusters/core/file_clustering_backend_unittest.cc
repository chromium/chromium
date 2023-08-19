// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/file_clustering_backend.h"
#include <memory>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

class FileClusteringBackendTest : public ::testing::Test {
 public:
  FileClusteringBackendTest() = default;
  ~FileClusteringBackendTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FileClusteringBackendTest, NoCommandLine) {
  std::unique_ptr<FileClusteringBackend> backend =
      FileClusteringBackend::CreateIfEnabled();
  EXPECT_EQ(backend, nullptr);
}

TEST_F(FileClusteringBackendTest, EmptyCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kClustersOverrideFile);

  std::unique_ptr<FileClusteringBackend> backend =
      FileClusteringBackend::CreateIfEnabled();
  EXPECT_EQ(backend, nullptr);
}

TEST_F(FileClusteringBackendTest, Success) {
  std::string clusters_json_string = R"(
      {
        "clusters": [
          {
            "visits": [
              {
                "visitId": "1",
                "score": 0.5
              },
              {
                "visitId": "2",
                "score": 0.3
              }
            ]
          },
          {
            "visits": [
              {
                "visitId": "3",
                "score": 0.5,
                "duplicateVisitIds": ["10", "11"],
                "imageUrl": "https://publicimage.com/image.jpg"
              },
              {
                "score": 0.5
              },
              {
                "visitId": "10"
              },
              {
                "visitId": "4",
                "score": 0.4
              }
            ]
          },
          {
            "visits": [
              {
                "visitId": "35",
                "score": 0.5
              },
              {
                "visitId": "36",
                "score": 0.4
              }
            ]
          }
        ]
      })";
  base::FilePath file_path =
      temp_dir().Append(FILE_PATH_LITERAL("clusters.json"));
  ASSERT_TRUE(base::WriteFile(file_path, clusters_json_string));

  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kClustersOverrideFile, file_path);

  std::unique_ptr<FileClusteringBackend> backend =
      FileClusteringBackend::CreateIfEnabled();
  ASSERT_NE(backend, nullptr);

  std::vector<history::AnnotatedVisit> annotated_visits;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://www.google.com/"));
  annotated_visits.push_back(visit);
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/"));
  annotated_visits.push_back(visit2);
  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://foo.com/"));
  annotated_visits.push_back(visit3);
  history::AnnotatedVisit visit10 =
      testing::CreateDefaultAnnotatedVisit(10, GURL("https://foo.com/"));
  annotated_visits.push_back(visit10);

  base::RunLoop run_loop;
  std::vector<history::Cluster> result_clusters;
  backend->GetClusters(ClusteringRequestSource::kJourneysPage,
                       base::BindOnce(
                           [](base::RunLoop* run_loop,
                              std::vector<history::Cluster>* out_clusters,
                              std::vector<history::Cluster> clusters) {
                             *out_clusters = std::move(clusters);
                             run_loop->Quit();
                           },
                           &run_loop, &result_clusters),
                       annotated_visits,
                       /*unused_requires_ui_and_triggerability=*/true);
  run_loop.Run();

  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.5),
                                      testing::VisitResult(2, 0.3)),
                          ElementsAre(testing::VisitResult(
                              3, 0.5, {history::DuplicateClusterVisit{10}}))));
  // Make sure visit URLs have URLs for display.
  for (const auto& result_cluster : result_clusters) {
    for (const auto& result_visit : result_cluster.visits) {
      EXPECT_FALSE(result_visit.url_for_display.empty());
    }
  }
  // Make sure image URL was parsed.
  EXPECT_EQ(result_clusters[1].visits[0].image_url.possibly_invalid_spec(),
            "https://publicimage.com/image.jpg");
}

TEST_F(FileClusteringBackendTest, FilterVisitsSuccess) {
  // Test setup:
  // Cluster override file has one cluster: cluster 1 -> visits 1.
  // Persistence has two clusters: cluster 1 -> visits 1,2 and cluster 2 ->
  // visit 3. Expects to return persisted cluster 1 -> visit 1.
  std::string clusters_json_string = R"(
      {
        "clusters": [
          {
            "visits": [
              {
                "visitId": "1",
                "score": 1.0
              }
            ]
          }
        ]
      })";
  base::FilePath file_path =
      temp_dir().Append(FILE_PATH_LITERAL("clusters_for_ui.json"));
  ASSERT_TRUE(base::WriteFile(file_path, clusters_json_string));

  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kClustersOverrideFile, file_path);

  std::unique_ptr<FileClusteringBackend> backend =
      FileClusteringBackend::CreateIfEnabled();
  ASSERT_NE(backend, nullptr);

  std::vector<history::ClusterVisit> cluster_visits1;
  history::AnnotatedVisit visit1 =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://www.google.com/"));
  history::ClusterVisit cluster_visit1 = testing::CreateClusterVisit(visit1);
  cluster_visits1.push_back(cluster_visit1);
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/"));
  history::ClusterVisit cluster_visit2 = testing::CreateClusterVisit(visit2);
  cluster_visits1.push_back(cluster_visit2);
  history::Cluster cluster1 = testing::CreateCluster(cluster_visits1);

  std::vector<history::ClusterVisit> cluster_visits2;
  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://www.youtube.com/"));
  history::ClusterVisit cluster_visit3 = testing::CreateClusterVisit(visit3);
  cluster_visits2.push_back(cluster_visit3);
  history::Cluster cluster2 = testing::CreateCluster(cluster_visits2);

  base::RunLoop run_loop;
  std::vector<history::Cluster> result_clusters;
  backend->GetClustersForUI(ClusteringRequestSource::kJourneysPage,
                            QueryClustersFilterParams(),
                            base::BindOnce(
                                [](base::RunLoop* run_loop,
                                   std::vector<history::Cluster>* out_clusters,
                                   std::vector<history::Cluster> clusters) {
                                  *out_clusters = std::move(clusters);
                                  run_loop->Quit();
                                },
                                &run_loop, &result_clusters),
                            {cluster1, cluster2});
  run_loop.Run();

  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0))));
}

}  // namespace
}  // namespace history_clusters
