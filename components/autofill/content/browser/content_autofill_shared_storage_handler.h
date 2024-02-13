// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_SHARED_STORAGE_HANDLER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_SHARED_STORAGE_HANDLER_H_

#include <memory>
#include <vector>

#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"

namespace autofill {

class CreditCard;

// Implements the AutofillSharedStorageHandler for the content layer.
class ContentAutofillSharedStorageHandler
    : public AutofillSharedStorageHandler {
 public:
  explicit ContentAutofillSharedStorageHandler(
      storage::SharedStorageManager& shared_storage_manager);
  ContentAutofillSharedStorageHandler(
      const ContentAutofillSharedStorageHandler&) = delete;
  ContentAutofillSharedStorageHandler& operator=(
      const ContentAutofillSharedStorageHandler&) = delete;
  ~ContentAutofillSharedStorageHandler() override;

  void OnServerCardDataRefreshed(const std::vector<std::unique_ptr<CreditCard>>&
                                     server_card_data) override;

 private:
  void ClearAutofillSharedStorageData();

  // Callback for shared storage results.
  void OnSharedStorageSetAutofillDataComplete(
      storage::SharedStorageManager::OperationResult result);

  // The shared storage manager that this instance uses. Must outlive this
  // instance.
  raw_ref<storage::SharedStorageManager> shared_storage_manager_;

  base::WeakPtrFactory<ContentAutofillSharedStorageHandler> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_SHARED_STORAGE_HANDLER_H_
