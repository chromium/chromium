// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_BULK_LEAK_CHECK_SERVICE_ADAPTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_BULK_LEAK_CHECK_SERVICE_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PrefService;

namespace password_manager {

// This class serves as an apdater for the BulkLeakCheckService and exposes an
// API that is intended to be consumed from the settings page.
class BulkLeakCheckServiceAdapter : public SavedPasswordsPresenter::Observer {
 public:
  BulkLeakCheckServiceAdapter(SavedPasswordsPresenter* presenter,
                              BulkLeakCheckServiceInterface* service,
                              PrefService* prefs);
  ~BulkLeakCheckServiceAdapter() override;

  // Instructs the adapter to start a check. This is a no-op in case a check is
  // already running. Otherwise, this will obtain the list of saved passwords
  // from |presenter_|, perform de-duplication of username and password pairs
  // and then feed it to the |service_| for checking. If |key| is present, it
  // will append |data->Clone()| to each created LeakCheckCredential.
  // Returns whether new check was started.
  bool StartBulkLeakCheck(LeakDetectionInitiator initiator,
                          const void* key = nullptr,
                          LeakCheckCredential::Data* data = nullptr);

  // This asks |service_| to stop an ongoing check.
  void StopBulkLeakCheck();

  // Obtains the state of the bulk leak check.
  BulkLeakCheckServiceInterface::State GetBulkLeakCheckState() const;

  // Gets the list of pending checks.
  size_t GetPendingChecksCount() const;

 private:
  // SavedPasswordsPresenter::Observer:
  void OnEdited(const CredentialUIEntry& form) override;

  // Weak handles to a presenter and service, respectively. These must be not
  // null and must outlive the adapter.
  raw_ptr<SavedPasswordsPresenter> presenter_ = nullptr;
  raw_ptr<BulkLeakCheckServiceInterface> service_ = nullptr;

  raw_ptr<PrefService> prefs_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_BULK_LEAK_CHECK_SERVICE_ADAPTER_H_
