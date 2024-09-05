// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/os_feedback_dialog/os_feedback_dialog.h"

#include <memory>
#include <utility>

#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"

namespace {

// All Feedback Tool window will be a fixed 600px*640dp portal per
// specification.
constexpr int kDialogWidth = 600;
constexpr int kDialogHeight = 640;

GURL GetUrl() {
  return GURL{ash::kChromeUIOSFeedbackUrl};
}

}  // namespace

namespace ash {

void OsFeedbackDialog::ShowDialogAsync(
    content::BrowserContext* context,
    const extensions::api::feedback_private::FeedbackInfo& info,
    base::OnceClosure callback,
    gfx::NativeWindow parent) {
  // If a dialog is opened, focus on it.
  auto* existing_instance =
      SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (existing_instance) {
    existing_instance->Focus();
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }
  // Metric should has been recorded for other request sources before
  // ShowDialogAsync is being called.
  if (info.flow == extensions::api::feedback_private::FeedbackFlow::kLogin) {
    UMA_HISTOGRAM_ENUMERATION("Feedback.RequestSource",
                              feedback::kFeedbackSourceLogin,
                              feedback::kFeedbackSourceCount);
  }

  // Take a screenshot and open the app afterward, regardless of screenshot
  // taking status. Screenshot is optional data.
  ash::OsFeedbackScreenshotManager::GetInstance()->TakeScreenshot(
      base::BindOnce(
          [](content::BrowserContext* context, base::Value::Dict feedback_info,
             base::OnceClosure callback, gfx::NativeWindow parent,
             bool /*taken_ok*/) {
            // The dialog will be self-destroyed when it is closed.
            auto* dialog = new OsFeedbackDialog(std::move(feedback_info));
            dialog->ShowSystemDialogForBrowserContext(context, parent);
            if (callback) {
              std::move(callback).Run();
            }
          },
          context, info.ToValue(), std::move(callback), parent));
}

gfx::NativeWindow OsFeedbackDialog::FindDialogWindow() {
  auto* instance = SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    return static_cast<OsFeedbackDialog*>(instance)->dialog_window();
  }
  return nullptr;
}

// Protected.
OsFeedbackDialog::OsFeedbackDialog(base::Value::Dict feedback_info)
    : SystemWebDialogDelegate(GetUrl(),
                              /* title=*/std::u16string()),
      feedback_info_(std::move(feedback_info)) {}

OsFeedbackDialog::~OsFeedbackDialog() = default;

// Private.
void OsFeedbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

std::string OsFeedbackDialog::GetDialogArgs() const {
  std::string data;
  base::JSONWriter::Write(feedback_info_, &data);
  return data;
}

}  // namespace ash
