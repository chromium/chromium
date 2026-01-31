// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_INTELLIGENT_SCAN_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_INTELLIGENT_SCAN_DELEGATE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ClientPhishingRequest;

// Delegate for handling intelligent scanning from Client Side Detection.
class IntelligentScanDelegate : public KeyedService {
 public:
  // The model type that the client uses to perform intelligent scan.
  enum class ModelType {
    kNotSupportedOnDevice = 0,
    kNotSupportedServerSide = 1,
    kOnDevice = 2,
    kServerSide = 3,
  };

  // Represents the result of an intelligent scan.
  struct IntelligentScanResult {
    static constexpr int kModelVersionUnavailable = -1;
    static IntelligentScanResult Success(std::string brand,
                                         std::string intent,
                                         int model_version,
                                         ModelType model_type);
    static IntelligentScanResult Failure(
        int model_version,
        ModelType model_type,
        IntelligentScanInfo::NoInfoReason no_info_reason);

    IntelligentScanResult();
    IntelligentScanResult(const IntelligentScanResult& other);
    IntelligentScanResult& operator=(const IntelligentScanResult& other);

    std::string brand;
    std::string intent;
    int model_version;
    bool execution_success;
    ModelType model_type;
    IntelligentScanInfo::NoInfoReason no_info_reason;
  };
  using IntelligentScanDoneCallback =
      base::OnceCallback<void(IntelligentScanResult)>;

  // Returns whether intelligent scan is available for the given model type.
  static bool IsIntelligentScanAvailable(ModelType model_type);

  ~IntelligentScanDelegate() override = default;

  // Determines if an intelligent scan should be requested based on the
  // verdict. Called from Client Side Detection.
  virtual bool ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) = 0;
  // Returns the model type that the client uses to perform intelligent scan.
  // Also logs failed eligibility reason histograms if
  // |log_failed_eligibility_reason| is true.
  virtual ModelType GetIntelligentScanModelType(
      bool log_failed_eligibility_reason) = 0;
  // Gets the intelligent scan result. The callback
  // will return an empty optional if intelligent scan is not available.
  // Returns a token that can be used to cancel the request. The token will be
  // std::nullopt in case the inquiry fails immediately without start.
  virtual std::optional<base::UnguessableToken> StartIntelligentScan(
      std::string rendered_texts,
      IntelligentScanDoneCallback callback) = 0;
  // Cancels a specific intelligent scan request. If the |scan_id| is
  // ongoing, it will return true, and false otherwise.
  virtual bool CancelIntelligentScan(const base::UnguessableToken& scan_id) = 0;
  // Determines if a CSD scam warning should be shown based on the intelligent
  // scan verdict.
  virtual bool ShouldShowScamWarning(
      std::optional<IntelligentScanVerdict> verdict) = 0;
  // Called when a CSD scam warning is shown based on an intelligent scan
  // verdict.
  virtual void OnScamWarningShown() {}
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_INTELLIGENT_SCAN_DELEGATE_H_
