// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/save_password_leak_detection_delegate.h"

#include "base/bind.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill_assistant {

SavePasswordLeakDetectionDelegate::SavePasswordLeakDetectionDelegate(
    password_manager::PasswordManagerClient* client)
    : client_(client),
      leak_factory_(
          std::make_unique<password_manager::LeakDetectionCheckFactoryImpl>()) {
}

SavePasswordLeakDetectionDelegate::~SavePasswordLeakDetectionDelegate() =
    default;

void SavePasswordLeakDetectionDelegate::StartLeakCheck(
    const password_manager::PasswordForm& credential,
    Callback callback,
    base::TimeDelta timeout) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAssistantAPCLeakCheckOnSaveSubmittedPassword)) {
    std::move(callback).Run(
        LeakDetectionStatus(LeakDetectionStatusCode::DISABLED_FOR_APC), false);
    return;
  }

  if (client_->IsIncognito()) {
    std::move(callback).Run(
        LeakDetectionStatus(LeakDetectionStatusCode::INCOGNITO_MODE), false);
    return;
  }

  if (!password_manager::CanStartLeakCheck(*client_->GetPrefs(), client_)) {
    std::move(callback).Run(
        LeakDetectionStatus(LeakDetectionStatusCode::DISABLED), false);
    return;
  }

  if (credential.username_value.empty()) {
    std::move(callback).Run(
        LeakDetectionStatus(LeakDetectionStatusCode::NO_USERNAME), false);
    return;
  }

  DCHECK(!credential.password_value.empty());
  // If there is already a leak check going on, terminate it.
  if (leak_check_) {
    leak_check_.reset();
    leak_detection_timer_.Stop();
    std::move(callback).Run(
        LeakDetectionStatus(LeakDetectionStatusCode::ABORTED), false);
  }

  leak_check_ = leak_factory_->TryCreateLeakCheck(
      this, client_->GetIdentityManager(), client_->GetURLLoaderFactory(),
      client_->GetChannel());
  if (!leak_check_) {
    std::move(callback).Run(LeakDetectionStatus(LeakDetectionStatusCode::OTHER),
                            false);
    return;
  }

  callback_ = std::move(callback);
  // TODO (crbug.com/1310169): Add a metric that measures turn-around time.
  leak_detection_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&SavePasswordLeakDetectionDelegate::OnLeakDetectionTimeout,
                     base::Unretained(this)));
  leak_check_->Start(credential.url, credential.username_value,
                     credential.password_value);
}

// Url, username and password parameters from the interface are not used.
void SavePasswordLeakDetectionDelegate::OnLeakDetectionDone(bool is_leaked,
                                                            GURL,
                                                            std::u16string,
                                                            std::u16string) {
  leak_detection_timer_.Stop();
  leak_check_.reset();
  std::move(callback_).Run(LeakDetectionStatus(), is_leaked);
}

void SavePasswordLeakDetectionDelegate::OnLeakDetectionTimeout() {
  leak_check_.reset();
  std::move(callback_).Run(
      LeakDetectionStatus(LeakDetectionStatusCode::TIMEOUT), false);
}

void SavePasswordLeakDetectionDelegate::OnError(
    password_manager::LeakDetectionError error) {
  leak_detection_timer_.Stop();
  leak_check_.reset();
  std::move(callback_).Run(LeakDetectionStatus(error), false);
}

}  // namespace autofill_assistant
