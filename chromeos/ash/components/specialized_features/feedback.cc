// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feedback.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version_info/version_string.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

namespace specialized_features {

namespace {

// Gets the current Chrome version as a string.
// Equivalent to `chrome::GetVersionString()`, without needing to use //chrome
// dependencies.
std::string GetChromeVersion() {
  return version_info::GetVersionStringWithModifier(ash::GetChannelName());
}

// Gets `CHROMEOS_RELEASE_VERSION` as a string.
std::string GetOsVersion() {
  std::string version;
  base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_VERSION", &version);
  return version;
}

// Redacts the description of the provided feedback data and returns it.
scoped_refptr<feedback::FeedbackData> RedactFeedbackData(
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  redaction::RedactionTool redactor(/*first_party_extension_ids=*/nullptr);
  redactor.EnableCreditCardRedaction(true);
  feedback_data->RedactDescription(redactor);
  return feedback_data;
}

// Queues a feedback report for the provided feedback data.
void SendFeedback(scoped_refptr<feedback::FeedbackData> feedback_data) {
  feedback_data->OnFeedbackPageDataComplete();
}

// Redacts the description, then sends the provided feedback data.
void RedactThenSendFeedback(
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RedactFeedbackData, std::move(feedback_data)),
      base::BindOnce(&SendFeedback));
}

}  // namespace

void SendFeedback(feedback::FeedbackUploader& uploader,
                  int product_id,
                  std::string description,
                  std::optional<std::string> image,
                  std::optional<std::string> image_mime_type) {
  auto feedback_data = base::MakeRefCounted<feedback::FeedbackData>(
      uploader.AsWeakPtr(), /*tracing_manager=*/nullptr);

  feedback_data->set_product_id(product_id);
  feedback_data->set_include_chrome_platform(false);
  feedback_data->set_description(std::move(description));
  if (image.has_value()) {
    feedback_data->set_image(std::move(*image));
  }
  if (image_mime_type.has_value()) {
    feedback_data->set_image_mime_type(std::move(*image_mime_type));
  }
  feedback_data->AddLog("CHROME VERSION", GetChromeVersion());
  feedback_data->AddLog("CHROMEOS_RELEASE_VERSION", GetOsVersion());

  RedactThenSendFeedback(feedback_data);
}

}  // namespace specialized_features
