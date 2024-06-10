// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_icon_diagnostic_command.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "ui/gfx/skia_util.h"

namespace web_app {

namespace {

base::Value CreateIconDiagnosticDebugData(
    const std::optional<WebAppIconDiagnosticResult> diagnostic_result) {
  if (!diagnostic_result.has_value()) {
    return base::Value("no_result_yet");
  }
  base::Value::Dict root;
  const WebAppIconDiagnosticResult result_value = diagnostic_result.value();
  root.Set("has_empty_downloaded_icon_sizes",
           result_value.has_empty_downloaded_icon_sizes);
  root.Set("has_generated_icon_flag", result_value.has_generated_icon_flag);
  root.Set("has_generated_icon_flag_false_negative",
           result_value.has_generated_icon_flag_false_negative);
  root.Set("has_generated_icon_bitmap", result_value.has_generated_icon_bitmap);
  root.Set("has_empty_icon_bitmap", result_value.has_empty_icon_bitmap);
  root.Set("has_empty_icon_file", result_value.has_empty_icon_file);
  root.Set("has_missing_icon_file", result_value.has_missing_icon_file);
  return base::Value(std::move(root));
}

}  // namespace

WebAppIconDiagnosticCommand::WebAppIconDiagnosticCommand(
    Profile* profile,
    const webapps::AppId& app_id,
    WebAppIconDiagnosticResultCallback result_callback)
    : web_app::WebAppCommand<AppLock,
                             std::optional<WebAppIconDiagnosticResult>>(
          "WebAppIconDiagnosticCommand",
          AppLockDescription(app_id),
          std::move(result_callback),
          /*args_for_shutdown=*/std::optional<WebAppIconDiagnosticResult>()),
      profile_(*profile),
      app_id_(app_id) {
  GetMutableDebugValue().Set("app_id", app_id);
}

WebAppIconDiagnosticCommand::~WebAppIconDiagnosticCommand() = default;

void WebAppIconDiagnosticCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  app_lock_ = std::move(lock);

  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  if (!web_app) {
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }

  const SortedSizesPx& downloaded_icon_sizes =
      web_app->downloaded_icon_sizes(IconPurpose::ANY);
  if (!downloaded_icon_sizes.empty()) {
    icon_size_ = *downloaded_icon_sizes.begin();
  }

  result_.emplace();
  result_->has_empty_downloaded_icon_sizes = downloaded_icon_sizes.empty();
  result_->has_generated_icon_flag = web_app->is_generated_icon();

  RunChainedCallbacks(
      base::BindOnce(&WebAppIconDiagnosticCommand::LoadIconFromProvider,
                     GetWeakPtr()),

      base::BindOnce(
          &WebAppIconDiagnosticCommand::DiagnoseGeneratedOrEmptyIconBitmap,
          GetWeakPtr()),

      base::BindOnce(
          &WebAppIconDiagnosticCommand::CheckForEmptyOrMissingIconFiles,
          GetWeakPtr()),

      base::BindOnce(
          &WebAppIconDiagnosticCommand::DiagnoseEmptyOrMissingIconFiles,
          GetWeakPtr()),

      base::BindOnce(&WebAppIconDiagnosticCommand::ReportResultAndDestroy,
                     GetWeakPtr()));
}

void WebAppIconDiagnosticCommand::LoadIconFromProvider(
    WebAppIconManager::ReadIconWithPurposeCallback callback) {
  if (!icon_size_) {
    std::move(callback).Run(IconPurpose::ANY, SkBitmap());
    return;
  }

  app_lock_->icon_manager().ReadSmallestIcon(
      app_id_, std::vector<IconPurpose>{IconPurpose::ANY}, *icon_size_,
      std::move(callback));
}

void WebAppIconDiagnosticCommand::DiagnoseGeneratedOrEmptyIconBitmap(
    base::OnceClosure done_callback,
    IconPurpose purpose,
    SkBitmap icon_bitmap) {
  if (icon_bitmap.drawsNothing()) {
    result_->has_empty_icon_bitmap = true;
    std::move(done_callback).Run();
    return;
  }

  const WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  if (!web_app) {
    std::move(done_callback).Run();
    return;
  }

  const std::string& name = web_app->untranslated_name();
  if (name.empty()) {
    std::move(done_callback).Run();
    return;
  }

  DCHECK(icon_size_);
  SkBitmap generated_icon_bitmap = shortcuts::GenerateBitmap(
      *icon_size_,
      shortcuts::GenerateIconLetterFromName(base::UTF8ToUTF16(name)));
  result_->has_generated_icon_bitmap =
      gfx::BitmapsAreEqual(icon_bitmap, generated_icon_bitmap);

  result_->has_generated_icon_flag_false_negative =
      !result_->has_generated_icon_flag && result_->has_generated_icon_bitmap;

  std::move(done_callback).Run();
}

void WebAppIconDiagnosticCommand::CheckForEmptyOrMissingIconFiles(
    base::OnceCallback<void(WebAppIconManager::IconFilesCheck)>
        icon_files_callback) {
  app_lock_->icon_manager().CheckForEmptyOrMissingIconFiles(
      app_id_, std::move(icon_files_callback));
}

void WebAppIconDiagnosticCommand::DiagnoseEmptyOrMissingIconFiles(
    base::OnceCallback<void(CommandResult)> done_callback,
    WebAppIconManager::IconFilesCheck icon_files_check) {
  result_->has_empty_icon_file = icon_files_check.empty > 0;
  result_->has_missing_icon_file = icon_files_check.missing > 0;
  std::move(done_callback).Run(CommandResult::kSuccess);
}

base::WeakPtr<WebAppIconDiagnosticCommand>
WebAppIconDiagnosticCommand::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppIconDiagnosticCommand::ReportResultAndDestroy(
    CommandResult command_result) {
  GetMutableDebugValue().Set(
      "command_result",
      command_result == CommandResult::kSuccess ? "success" : "failure");
  GetMutableDebugValue().Set("icon_diagnostic_results",
                             CreateIconDiagnosticDebugData(result_));
  CompleteAndSelfDestruct(command_result, result_);
}

}  // namespace web_app
