// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_feedback.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/os_feedback_dialog/os_feedback_dialog.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback

LoginFeedback::LoginFeedback(Profile* signin_profile)
    : profile_(signin_profile) {}

LoginFeedback::~LoginFeedback() {}

void LoginFeedback::Request(const std::string& description) {
  Request(description, base::NullCallback());
}

void LoginFeedback::Request(const std::string& description,
                            base::OnceClosure callback) {
  description_ = description;

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile_);

  auto info = api->CreateFeedbackInfo(
      description_, std::string(), "Login", std::string(), GURL(),
      extensions::api::feedback_private::FeedbackFlow::kLogin,
      /*from_assistant=*/false,
      /*include_bluetooth_logs=*/false,
      /*show_questionnaire=*/false,
      /*from_chrome_labs_or_kaleidoscope=*/false,
      /*from_autofill=*/false,
      /*autofill_metadata=*/base::Value::Dict(),
      /*ai_metadata=*/base::Value::Dict());

  if (ash::features::IsOsFeedbackDialogEnabled()) {
    OsFeedbackDialog::ShowDialogAsync(profile_, *info, std::move(callback));
  } else {
    FeedbackDialog::CreateOrShow(profile_, *info);
  }
}

}  // namespace ash
