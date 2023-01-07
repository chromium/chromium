// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_ASSIST_RANKER_SERVICE_IMPL_H_
#define COMPONENTS_ASSIST_RANKER_ASSIST_RANKER_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/assist_ranker/assist_ranker_service.h"
#include "components/assist_ranker/predictor_config.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace assist_ranker {

class BasePredictor;
class BinaryClassifierPredictor;

class AssistRankerServiceImpl : public AssistRankerService {
 public:
  AssistRankerServiceImpl(
      base::FilePath base_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  AssistRankerServiceImpl(const AssistRankerServiceImpl&) = delete;
  AssistRankerServiceImpl& operator=(const AssistRankerServiceImpl&) = delete;

  ~AssistRankerServiceImpl() override;

  // AssistRankerService...
  base::WeakPtr<BinaryClassifierPredictor> FetchBinaryClassifierPredictor(
      const PredictorConfig& config) override;

 private:
  // Returns the full path to the model cache.
  base::FilePath GetModelPath(const std::string& model_filename);

  // URL loader factory used for RankerURLFetcher.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Base path where models are stored.
  const base::FilePath base_path_;

  std::unordered_map<std::string, std::unique_ptr<BasePredictor>>
      predictor_map_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_ASSIST_RANKER_SERVICE_IMPL_H_
