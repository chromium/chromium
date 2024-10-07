// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/assist_ranker_service_impl.h"
#include "base/memory/weak_ptr.h"
#include "components/assist_ranker/binary_classifier_predictor.h"
#include "components/assist_ranker/ranker_model_loader_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace assist_ranker {

AssistRankerServiceImpl::AssistRankerServiceImpl(
    base::FilePath base_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      base_path_(std::move(base_path)) {}

AssistRankerServiceImpl::~AssistRankerServiceImpl() = default;

base::WeakPtr<BinaryClassifierPredictor>
AssistRankerServiceImpl::FetchBinaryClassifierPredictor(
    const PredictorConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& model_name = config.model_name;
  auto predictor_it = predictor_map_.find(model_name);
  if (predictor_it != predictor_map_.end()) {
    DVLOG(1) << "Predictor " << model_name << " already initialized.";
    return static_cast<BinaryClassifierPredictor*>(predictor_it->second.get())
        ->AsWeakPtr();
  }

  // The predictor does not exist yet, so we create one.
  DVLOG(1) << "Initializing predictor: " << model_name;
  std::unique_ptr<BinaryClassifierPredictor> predictor =
      BinaryClassifierPredictor::Create(config, GetModelPath(model_name),
                                        url_loader_factory_);
  base::WeakPtr<BinaryClassifierPredictor> weak_ptr = predictor->AsWeakPtr();
  predictor_map_[model_name] = std::move(predictor);
  return weak_ptr;
}

base::FilePath AssistRankerServiceImpl::GetModelPath(
    const std::string& model_filename) {
  if (base_path_.empty())
    return base::FilePath();
  return base_path_.AppendASCII(model_filename);
}

}  // namespace assist_ranker
