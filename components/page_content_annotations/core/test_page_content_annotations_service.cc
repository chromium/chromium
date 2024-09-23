// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/test_page_content_annotations_service.h"

#include "base/task/sequenced_task_runner.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"

namespace page_content_annotations {

// static
std::unique_ptr<TestPageContentAnnotationsService>
TestPageContentAnnotationsService::Create(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_model_provider,
    history::HistoryService* history_service) {
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider;
  optimization_guide::OptimizationGuideModelProvider* model_provider_to_use =
      optimization_guide_model_provider;
  if (!model_provider_to_use) {
    test_model_provider = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    model_provider_to_use = test_model_provider.get();
  }

  std::unique_ptr<base::ScopedTempDir> temp_dir;
  std::unique_ptr<history::HistoryService> test_history_service;
  history::HistoryService* history_service_to_use = history_service;
  if (!history_service_to_use) {
    temp_dir = std::make_unique<base::ScopedTempDir>();
    CHECK(temp_dir->CreateUniqueTempDir());
    test_history_service = std::make_unique<history::HistoryService>();
    test_history_service->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir->GetPath()));
    history_service_to_use = test_history_service.get();
  }

  auto test_service = base::WrapUnique(new TestPageContentAnnotationsService(
      model_provider_to_use, history_service_to_use));
  test_service->temp_dir_ = std::move(temp_dir);
  test_service->test_model_provider_ = std::move(test_model_provider);
  test_service->test_history_service_ = std::move(test_history_service);

  return test_service;
}

TestPageContentAnnotationsService::~TestPageContentAnnotationsService() {
  if (test_history_service_) {
    // Delete the history service on the next message pump so that PCAService's
    // |ScopedObservation| has a chance to be deleted first.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(test_history_service_));
  }

  // Delete the model provider on the next message pump so that PCAService's
  // ModelHandlers have a chance to be deleted first and remove their
  // observations first.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(test_model_provider_));
}

TestPageContentAnnotationsService::TestPageContentAnnotationsService(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_model_provider,
    history::HistoryService* history_service)
    : PageContentAnnotationsService(
          /*application_locale=*/"en-US",
          /*country_code=*/"US",
          optimization_guide_model_provider,
          history_service,
          /*template_url_service=*/nullptr,
          /*zero_suggest_cache_service=*/nullptr,
          /*database_provider=*/nullptr,
          /*database_dir=*/base::FilePath(),
          /*optimization_guide_logger=*/nullptr,
          /*optimization_guide_decider=*/nullptr,
          /*background_task_runner=*/nullptr) {}

}  // namespace page_content_annotations
