// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/apc_internals/apc_internals_logins_request.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"

APCInternalsLoginsRequest::APCInternalsLoginsRequest(
    base::OnceCallback<void(const GURL& url, const std::string& username)>
        on_success_callback,
    base::OnceCallback<void(APCInternalsLoginsRequest*)>
        request_finished_callback)
    : on_success_callback_(std::move(on_success_callback)),
      request_finished_callback_(std::move(request_finished_callback)) {}

APCInternalsLoginsRequest::~APCInternalsLoginsRequest() = default;

void APCInternalsLoginsRequest::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  for (auto& password_form : results)
    results_.push_back(std::move(password_form));

  if (--wait_counter_ > 0)
    return;

  if (!results_.empty()) {
    GURL url = results_.front()->url;
    std::string username = base::UTF16ToUTF8(results_.front()->username_value);
    std::move(on_success_callback_).Run(url, username);
  }

  std::move(request_finished_callback_).Run(this);
}

void APCInternalsLoginsRequest::IncreaseWaitCounter() {
  wait_counter_++;
}

base::WeakPtr<APCInternalsLoginsRequest>
APCInternalsLoginsRequest::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
