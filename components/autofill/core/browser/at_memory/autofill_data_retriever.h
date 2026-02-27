// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_RETRIEVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_RETRIEVER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

class AutofillDataRetriever {
 public:
  explicit AutofillDataRetriever(AutofillClient& client);
  ~AutofillDataRetriever();

  // Retrieves all entities for a given data type.
  std::vector<MemorySearchResult> RetrieveAll(AtMemoryDataType intent);

 private:
  const raw_ref<AutofillClient> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_RETRIEVER_H_
