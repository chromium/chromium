// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_LOGINS_REQUEST_H_
#define CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_LOGINS_REQUEST_H_

#include "components/password_manager/core/browser/password_store_consumer.h"
#include "url/gurl.h"

// Helper class for fetching logins from password store.
class APCInternalsLoginsRequest
    : public password_manager::PasswordStoreConsumer {
 public:
  explicit APCInternalsLoginsRequest(
      base::OnceCallback<void(const GURL& url, const std::string& username)>
          on_success_callback,
      base::OnceCallback<void(APCInternalsLoginsRequest*)>
          request_finished_callback);

  ~APCInternalsLoginsRequest() override;

  // Called by PasswordStoreInterface::GetLogins on completion.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  // Increase `wait_counter_` by 1.
  void IncreaseWaitCounter();

  base::WeakPtr<APCInternalsLoginsRequest> GetWeakPtr();

 private:
  // Callback for when all password stores are finished retrieving logins and
  // there is at least one login. Used for launching a script.
  base::OnceCallback<void(const GURL& url, const std::string& username)>
      on_success_callback_;

  // Callback for when all password stores are finished retrieving logins. Used
  // for clearing requests queue (outside of this class).
  base::OnceCallback<void(APCInternalsLoginsRequest*)>
      request_finished_callback_;

  // The number of password stores this class is still waiting for to complete
  // retrieving logins.
  int wait_counter_ = 0;

  // Logins retrieved from all password stores.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> results_;

  base::WeakPtrFactory<APCInternalsLoginsRequest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_LOGINS_REQUEST_H_
