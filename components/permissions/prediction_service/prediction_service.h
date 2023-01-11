// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/prediction_service_base.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace permissions {

// TODO(crbug.com/1138595, andypaicu): Refactor this class and
// RealTimeUrlLookupServiceBase to derive from the same base class instead of
// doing a bunch of duplicate work. Design doc:
// go/permissions-predictions-client-doc

// Service used to makes calls to the Web Permission Suggestions Service to
// obtaing recomandations regarding permission prompts.
class PredictionService : public PredictionServiceBase {
 public:
  using PendingRequestsMap = std::map<std::unique_ptr<network::SimpleURLLoader>,
                                      LookupResponseCallback>;
  explicit PredictionService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PredictionService() override;

  void StartLookup(const PredictionRequestFeatures& entity,
                   LookupRequestCallback request_callback,
                   LookupResponseCallback response_callback) override;

  void set_prediction_service_url_for_testing(const GURL& url) {
    prediction_service_url_override_ = url;
  }

  const PendingRequestsMap& pending_requests_for_testing() {
    return pending_requests_;
  }

  void recalculate_service_url_every_time_for_testing() {
    recalculate_service_url_every_time = true;
  }

 private:
  static const GURL GetPredictionServiceUrl(bool recalculate_for_testing);
  std::unique_ptr<network::ResourceRequest> GetResourceRequest();

  void SendRequestInternal(std::unique_ptr<network::ResourceRequest> request,
                           const std::string& request_data,
                           const PredictionRequestFeatures& entity,
                           LookupResponseCallback response_callback);
  void OnURLLoaderComplete(const PredictionRequestFeatures& entity,
                           network::SimpleURLLoader* loader,
                           base::TimeTicks request_start_time,
                           std::unique_ptr<std::string> response_body);
  std::unique_ptr<GeneratePredictionsResponse> CreatePredictionsResponse(
      network::SimpleURLLoader* loader,
      const std::string* response_body);

  PendingRequestsMap pending_requests_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GURL prediction_service_url_override_;
  bool recalculate_service_url_every_time = false;

  base::WeakPtrFactory<PredictionService> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_H_
