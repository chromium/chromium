// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/edu_coexistence_consent_tracker.h"

#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

// static
EduCoexistenceConsentTracker* EduCoexistenceConsentTracker::Get() {
  static base::NoDestructor<EduCoexistenceConsentTracker> instance;
  return instance.get();
}

EduCoexistenceConsentTracker::EmailAndCallback::EmailAndCallback() = default;

EduCoexistenceConsentTracker::EmailAndCallback::~EmailAndCallback() = default;

EduCoexistenceConsentTracker::EduCoexistenceConsentTracker() = default;

EduCoexistenceConsentTracker::~EduCoexistenceConsentTracker() = default;

void EduCoexistenceConsentTracker::WaitForEduConsent(
    const content::WebUI* web_ui,
    const std::string& account_email,
    base::OnceCallback<void(bool)> callback) {
  if (consent_tracker_[web_ui].received_consent) {
    DCHECK_EQ(consent_tracker_[web_ui].email, account_email);
    std::move(callback).Run(/* success */ true);
    return;
  }

  consent_tracker_[web_ui].email = account_email;
  consent_tracker_[web_ui].callback = std::move(callback);
}

void EduCoexistenceConsentTracker::OnDialogClosed(
    const content::WebUI* web_ui) {
  if (!base::Contains(consent_tracker_, web_ui))
    return;

  if (consent_tracker_.at(web_ui).callback) {
    std::move(consent_tracker_[web_ui].callback).Run(/* success */ false);
  }

  consent_tracker_.erase(web_ui);
}

void EduCoexistenceConsentTracker::OnConsentLogged(
    const content::WebUI* web_ui,
    const std::string& account_email) {
  if (consent_tracker_[web_ui].callback) {
    DCHECK_EQ(consent_tracker_[web_ui].email, account_email);
    std::move(consent_tracker_[web_ui].callback).Run(/* success */ true);
    return;
  }

  consent_tracker_[web_ui].received_consent = true;
  consent_tracker_[web_ui].email = account_email;
}

const EduCoexistenceConsentTracker::EmailAndCallback*
EduCoexistenceConsentTracker::GetInfoForWebUIForTest(
    const content::WebUI* web_ui) {
  if (!base::Contains(consent_tracker_, web_ui))
    return nullptr;
  return &consent_tracker_[web_ui];
}

}  // namespace chromeos
