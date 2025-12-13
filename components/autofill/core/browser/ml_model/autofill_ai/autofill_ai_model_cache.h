// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/autofill_ai_model_cache.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill {

// `AutofillAiModelCache` is an interface for storing and retrieving AutofillAI
// model responses. The cache is per profile.
class AutofillAiModelCache : public KeyedService {
 public:
  using ModelResponse = optimization_guide::proto::AutofillAiTypeResponse;
  using CacheEntryWithMetadata = AutofillAiModelCacheEntryWithMetadata;

  // Identifies a field within a form.
  struct FieldIdentifier {
    FieldSignature signature;
    size_t rank_in_signature_group = 0;

    friend constexpr bool operator==(const FieldIdentifier&,
                                     const FieldIdentifier&) = default;
    friend constexpr auto operator<=>(const FieldIdentifier&,
                                      const FieldIdentifier&) = default;
  };

  // The part of the model response relevant for type predictions in Autofill.
  struct FieldPrediction final {
    FieldPrediction();
    explicit FieldPrediction(
        FieldType type,
        std::optional<AutofillFormatString> format_string = std::nullopt);
    FieldPrediction(const FieldPrediction&);
    FieldPrediction& operator=(const FieldPrediction&);
    FieldPrediction(FieldPrediction&&);
    FieldPrediction& operator=(FieldPrediction&&);
    ~FieldPrediction();

    FieldType field_type = NO_SERVER_DATA;
    std::optional<AutofillFormatString> format_string;

    friend constexpr bool operator==(const FieldPrediction&,
                                     const FieldPrediction&) = default;
  };

  // Updates the entry with key `form_signature`. If the `form_signature` is
  // not yet known to the cache, it is added to it.
  // `field_identifiers` must have the same size as `response.field_responses`.
  virtual void Update(FormSignature form_signature,
                      ModelResponse response,
                      base::span<const FieldIdentifier> field_identifiers) = 0;

  // Returns whether the cache contains an entry with `form_signature`.
  virtual bool Contains(FormSignature form_signature) const = 0;

  // Removes the cache entry with `form_signature`. No-op if no such entry
  // exists.
  virtual void Erase(FormSignature form_signature) = 0;

  // Returns the entire content of the cache, including metadata (such as
  // creation dates).
  virtual std::map<FormSignature, CacheEntryWithMetadata> GetAllEntries()
      const = 0;

  // Returns a "parsed" version of the field predictions for a given
  // `form_signature`. Returns an empty map if there is no cache entry for
  // `form_signature`.
  virtual base::flat_map<FieldIdentifier, FieldPrediction> GetFieldPredictions(
      FormSignature form_signature) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_H_
