// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_types.h"
#include "url/gurl.h"

namespace personal_context {
class PersonalContextService;
}

namespace device_reauth {
class DeviceAuthenticator;
}

namespace autofill {

class AutofillClient;
class AutofillDataProvider;

// Service for querying @memory suggestions. Owned by the Profile, one per
// profile.
class AtMemoryQueryService : public KeyedService {
 public:
  // Reasons for unmasking or authentication failure when retrieving PII.
  enum class SpiiRetrievalFailureReason {
    // There is no network connection.
    kNoConnection,
    // Biometric/screen-lock authentication failed or was cancelled.
    kReauthFailed,
    // Another authentication request is already in progress.
    kReauthInProgress,
    // The request to the `PersonalContextService` failed.
    kFetchFailed,
    // The server response could not be parsed.
    kParseFailed
  };

  using SpiiRetrievalResult =
      base::expected<std::u16string, SpiiRetrievalFailureReason>;

  using FetchUnmaskedPiiEntitiesCallback =
      base::OnceCallback<void(SpiiRetrievalResult)>;

  AtMemoryQueryService(
      std::unique_ptr<AutofillDataProvider> data_provider,
      personal_context::PersonalContextService* personal_context_service,
      const std::string& locale);
  AtMemoryQueryService(const AtMemoryQueryService&) = delete;
  AtMemoryQueryService& operator=(const AtMemoryQueryService&) = delete;
  ~AtMemoryQueryService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes a server query, using user provided `query` and returns search
  // results via `callback`.
  virtual void Query(
      std::u16string_view query,
      const GURL& url,
      std::u16string_view title,
      base::RepeatingCallback<
          void(accessibility_annotator::MemorySearchResults)> callback);

  // Authenticates the user and then fetches the unmasked PII entities from the
  // server. Fails if an authentication request is already in progress.
  // `callback` is always called asynchronously. If successful, `callback` is
  // called with an unobfuscated value for `data_type`, otherwise an error
  // reason is provided.
  // TODO(crbug.com/525385681): Use in `AtMemoryManager` before filling
  // suggestions.
  virtual void AuthenticateAndFetchPiiEntity(
      const AutofillClient& client,
      const std::u16string& auth_message,
      std::u16string_view masked_value,
      accessibility_annotator::MemoryDataType data_type,
      base::span<const accessibility_annotator::EntryMetadata> metadata_list,
      FetchUnmaskedPiiEntitiesCallback callback);

 private:
  // Called when the PersonalContextService query returns.
  void OnPersonalContextRetrieved(
      base::RepeatingCallback<
          void(accessibility_annotator::MemorySearchResults)> callback,
      personal_context::FetchContextResult result);

  // Called when the local data provider finishes retrieving local memory
  // entries specified in the fetch plan. It filters these local entries,
  // ranks them with the remote results, deduplicates them, and reports the
  // final results via `callback`.
  void OnLocalDataRetrieved(
      base::RepeatingCallback<
          void(accessibility_annotator::MemorySearchResults)> callback,
      std::vector<accessibility_annotator::MemorySearchResult> remote_results,
      base::flat_set<std::u16string> filter_words,
      std::string server_request_id,
      std::vector<accessibility_annotator::MemorySearchResult> local_results);

  // Called when the authentication is completed.
  // Performs the final PII unmasking request to `PersonalContextService` if
  // authentication succeeded.
  void OnAuthenticationCompleted(
      std::u16string masked_value,
      accessibility_annotator::MemoryDataType data_type,
      std::vector<accessibility_annotator::EntryMetadata> metadata_list,
      FetchUnmaskedPiiEntitiesCallback callback,
      bool auth_succeeded);

  std::unique_ptr<AutofillDataProvider> data_provider_;
  raw_ptr<personal_context::PersonalContextService> personal_context_service_ =
      nullptr;
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;
  std::string locale_;
  base::WeakPtrFactory<AtMemoryQueryService> query_weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemoryQueryService> pii_unmasking_weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
