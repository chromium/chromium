// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SHARED_STORAGE_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SHARED_STORAGE_HANDLER_H_

#include <memory>
#include <vector>

namespace autofill {

class CreditCard;

// Abstract class that handles interacting with the SharedStorageManager on
// supported platforms.
//
// On Desktop and Android, ContentAutofillSharedStorageHandler (in
// components/autofill/content/) extends this class, and handles interactions
// with the SharedStorageManager.
//
// On iOS, SharedStorage is not supported, so the behavior is not implemented.
class AutofillSharedStorageHandler {
 public:
  virtual ~AutofillSharedStorageHandler() = default;

  virtual void OnServerCardDataRefreshed(
      const std::vector<std::unique_ptr<CreditCard>>& server_card_data) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SHARED_STORAGE_HANDLER_H_
