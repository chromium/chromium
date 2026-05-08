// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_GEMINI_GEMINI_STATUS_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_GEMINI_GEMINI_STATUS_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

class PrefService;

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

class PrefRegistrySimple;

namespace ash::boca {

class GeminiStatusFetcher {
 public:
  using GetStatusCallback = base::OnceCallback<void(bool)>;

  GeminiStatusFetcher(
      std::string gaia_id,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service);

  GeminiStatusFetcher(const GeminiStatusFetcher&) = delete;
  GeminiStatusFetcher& operator=(const GeminiStatusFetcher&) = delete;

  ~GeminiStatusFetcher();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Requests the Gemini status. If a request is already in progress, the
  // callback is queued and will be notified when the active request completes.
  void GetStatus(GetStatusCallback callback);

 private:
  void GetStatusInternal();
  void OnStatusResponse(std::optional<bool> result);

  const std::string gaia_id_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<PrefService> pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<google_apis::RequestSender> request_sender_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool request_in_progress_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::queue<GetStatusCallback> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<GeminiStatusFetcher> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_GEMINI_GEMINI_STATUS_FETCHER_H_
