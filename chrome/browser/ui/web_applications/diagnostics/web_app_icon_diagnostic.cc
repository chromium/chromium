// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_diagnostic.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "ui/gfx/skia_util.h"

namespace web_app {

WebAppIconDiagnostic::WebAppIconDiagnostic(Profile* profile, AppId app_id)
    : profile_(profile),
      app_id_(std::move(app_id)),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_.get())),
      app_(provider_->registrar_unsafe().GetAppById(app_id_)) {}

WebAppIconDiagnostic::~WebAppIconDiagnostic() = default;

void WebAppIconDiagnostic::Run(
    base::OnceCallback<void(absl::optional<Result>)> result_callback) {
  result_callback_ = std::move(result_callback);

  if (!app_) {
    CallResultCallback();
    return;
  }

  const SortedSizesPx& downloaded_icon_sizes =
      app_->downloaded_icon_sizes(IconPurpose::ANY);
  if (!downloaded_icon_sizes.empty())
    icon_size_ = *downloaded_icon_sizes.begin();

  result_.emplace();
  result_->has_empty_downloaded_icon_sizes = downloaded_icon_sizes.empty();
  result_->has_generated_icon_flag = app_->is_generated_icon();

  RunChainedCallbacks(
      base::BindOnce(&WebAppIconDiagnostic::LoadIconFromProvider, GetWeakPtr()),

      base::BindOnce(&WebAppIconDiagnostic::DiagnoseGeneratedOrEmptyIconBitmap,
                     GetWeakPtr()),

      base::BindOnce(&WebAppIconDiagnostic::CheckForEmptyOrMissingIconFiles,
                     GetWeakPtr()),

      base::BindOnce(&WebAppIconDiagnostic::DiagnoseEmptyOrMissingIconFiles,
                     GetWeakPtr()),

      base::BindOnce(&WebAppIconDiagnostic::CallResultCallback, GetWeakPtr()));
}

base::WeakPtr<WebAppIconDiagnostic> WebAppIconDiagnostic::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppIconDiagnostic::CallResultCallback() {
  std::move(result_callback_).Run(std::move(result_));
}

void WebAppIconDiagnostic::LoadIconFromProvider(
    WebAppIconManager::ReadIconWithPurposeCallback callback) {
  if (!icon_size_) {
    std::move(callback).Run(IconPurpose::ANY, SkBitmap());
    return;
  }

  provider_->icon_manager().ReadSmallestIcon(
      app_id_, std::vector<IconPurpose>{IconPurpose::ANY}, *icon_size_,
      std::move(callback));
}

void WebAppIconDiagnostic::DiagnoseGeneratedOrEmptyIconBitmap(
    base::OnceClosure done_callback,
    IconPurpose purpose,
    SkBitmap icon_bitmap) {
  if (icon_bitmap.drawsNothing()) {
    result_->has_empty_icon_bitmap = true;
    std::move(done_callback).Run();
    return;
  }

  const std::string& name = app_->untranslated_name();
  if (name.empty()) {
    std::move(done_callback).Run();
    return;
  }

  DCHECK(icon_size_);
  SkBitmap generated_icon_bitmap = GenerateBitmap(
      *icon_size_, GenerateIconLetterFromAppName(base::UTF8ToUTF16(name)));
  result_->has_generated_icon_bitmap =
      gfx::BitmapsAreEqual(icon_bitmap, generated_icon_bitmap);

  result_->has_generated_icon_flag_false_negative =
      !result_->has_generated_icon_flag && result_->has_generated_icon_bitmap;

  std::move(done_callback).Run();
}

void WebAppIconDiagnostic::CheckForEmptyOrMissingIconFiles(
    base::OnceCallback<void(WebAppIconManager::IconFilesCheck)>
        icon_files_callback) {
  provider_->icon_manager().CheckForEmptyOrMissingIconFiles(
      app_id_, std::move(icon_files_callback));
}

void WebAppIconDiagnostic::DiagnoseEmptyOrMissingIconFiles(
    base::OnceClosure done_callback,
    WebAppIconManager::IconFilesCheck icon_files_check) {
  result_->has_empty_icon_file = icon_files_check.empty > 0;
  result_->has_missing_icon_file = icon_files_check.missing > 0;
  std::move(done_callback).Run();
}

std::ostream& operator<<(std::ostream& os,
                         const WebAppIconDiagnostic::Result result) {
  os << "has_empty_downloaded_icon_sizes: "
     << result.has_empty_downloaded_icon_sizes << std::endl;
  os << "has_generated_icon_flag: " << result.has_generated_icon_flag
     << std::endl;
  os << "has_generated_icon_flag_false_negative: "
     << result.has_generated_icon_flag_false_negative << std::endl;
  os << "has_generated_icon_bitmap: " << result.has_generated_icon_bitmap
     << std::endl;
  os << "has_empty_icon_bitmap: " << result.has_empty_icon_bitmap << std::endl;
  os << "has_empty_icon_file: " << result.has_empty_icon_file << std::endl;
  os << "has_missing_icon_file: " << result.has_missing_icon_file << std::endl;
  return os;
}

}  // namespace web_app
