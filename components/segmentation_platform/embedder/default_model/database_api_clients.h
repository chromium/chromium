// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DATABASE_API_CLIENTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DATABASE_API_CLIENTS_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model to register signals for DatabaseApiClients. A model is used as a
// workaround to force the observation, storing and deletion of signals needed
// for database API clients. This should ideally be moved inside the service.
class DatabaseApiClients : public DefaultModelProvider {
 public:
  static constexpr char kDatabaseApiClientsKey[] = "database_api_clients";
  static constexpr char kDatabaseApiClientsUmaName[] = "DatabaseApiClients";

  DatabaseApiClients();
  ~DatabaseApiClients() override = default;

  DatabaseApiClients(const DatabaseApiClients&) = delete;
  DatabaseApiClients& operator=(const DatabaseApiClients&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // Helper to write a query to sum the `metric_name` for the past `days`.
  static void AddSumQuery(MetadataWriter& writer,
                          std::string_view metric_name,
                          int days);

  // Helper to write a query to get the sum of metric values for a given
  // `event_name` and a collection of `metric_names` for the past `days`.
  static void AddSumGroupQuery(MetadataWriter& writer,
                               std::string_view event_name,
                               const std::set<std::string>& metric_names,
                               int days);

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DATABASE_API_CLIENTS_H_
