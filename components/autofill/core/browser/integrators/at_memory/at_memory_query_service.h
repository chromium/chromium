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
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
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
  // LINT.IfChange(SpiiRetrievalFailureReason)
  // Reasons for unmasking or authentication failure when retrieving PII.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SpiiRetrievalFailureReason {
    // There is no network connection.
    kNoConnection = 0,
    // Biometric/screen-lock authentication failed or was cancelled.
    kReauthFailed = 1,
    // Another authentication request is already in progress.
    kReauthInProgress = 2,
    // The request to the `PersonalContextService` failed.
    kFetchFailed = 3,
    // The server response could not be parsed.
    kParseFailed = 4,
    kMaxValue = kParseFailed
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAtMemorySpiiRetrievalFailureReason)

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
      base::RepeatingCallback<void(MemorySearchResults)> callback);

  // Authenticates the user and then fetches the unmasked PII entities from the
  // server. Fails if an authentication request is already in progress.
  // `callback` is always called asynchronously. If successful, `callback` is
  // called with an unobfuscated value for `data_type`, otherwise an error
  // reason is provided.
  virtual void AuthenticateAndFetchPiiEntity(
      const AutofillClient& client,
      const std::u16string& auth_message,
      std::u16string_view masked_value,
      MemoryDataType data_type,
      base::span<const EntryMetadata> metadata_list,
      FetchUnmaskedPiiEntitiesCallback callback);

 private:
  // Called when the PersonalContextService query returns.
  void OnPersonalContextRetrieved(
      base::RepeatingCallback<void(MemorySearchResults)> callback,
      personal_context::FetchContextResult result);

  // Called when the local data provider finishes retrieving local memory
  // entries specified in the fetch plan. It filters these local entries,
  // ranks them with the remote results, deduplicates them, and reports the
  // final results via `callback`.
  void OnLocalDataRetrieved(
      base::RepeatingCallback<void(MemorySearchResults)> callback,
      std::vector<MemorySearchResult> remote_results,
      base::flat_set<std::u16string> filter_words,
      std::string server_request_id,
      std::vector<MemorySearchResult> local_results);

  // Called when the authentication is completed.
  // Performs the final PII unmasking request to `PersonalContextService` if
  // authentication succeeded.
  void OnAuthenticationCompleted(std::u16string masked_value,
                                 MemoryDataType data_type,
                                 std::vector<EntryMetadata> metadata_list,
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

// TODO(crbug.com/542022101): Move all of these functions into the anonymous
// namespace once they can be tested via `AtMemoryQueryService::Query()`.
namespace internal {

// Returns whether `entry_string` matches `filter` according to its filter mode.
bool MatchesStringFilter(
    std::u16string_view entry_string,
    const personal_context::proto::AutofillFetchSpecification::StringFilter&
        filter);

// Returns whether `entry_typed_val` matches `filter`.
bool MatchesTypedFilter(
    const personal_context::proto::TypedValue& entry_typed_val,
    const personal_context::proto::AutofillFetchSpecification::TypedValueFilter&
        filter);

// Returns whether `entry` or any of its metadata items whose type is allowed
// by `filter.data_types()` matches `filter.typed_value_filter()` (when set) or
// `filter.string_filter()`.
bool MatchesFilter(
    const MemorySearchResult& entry,
    const personal_context::proto::AutofillFetchSpecification::Filter& filter);

// Returns whether `entry` has the data type requested by `spec` and satisfies
// all filters in `spec.filters()`.
bool MatchesFetchSpecification(
    const MemorySearchResult& entry,
    const personal_context::proto::AutofillFetchSpecification& spec);

}  // namespace internal

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
