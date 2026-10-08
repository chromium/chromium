// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_install_dialog_view_android.h"

#include <jni.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/install_prompt_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionInstallDialogBridge_jni.h"

using extensions::InstallPromptData;

namespace {

void ShowExtensionInstallDialogAndroid(
    std::unique_ptr<ExtensionInstallPromptShowParams> show_params,
    ExtensionInstallPrompt::DoneCallback done_callback,
    std::unique_ptr<InstallPromptData> prompt) {
  // A parent window is required to retrieve the ModalDialogManager. If
  // show_params was configured with a WebContents, GetParentWindow() computes
  // the window from the WebContents's top-level native window.
  ui::WindowAndroid* window_android = show_params->GetParentWindow();
  if (!window_android) {
    LOG(ERROR) << "Parent window not found.";
    if (done_callback) {
      std::move(done_callback)
          .Run(ExtensionInstallPrompt::DoneCallbackPayload(
              ExtensionInstallPrompt::Result::ABORTED));
    }
    return;
  }

  // WebContents is optional (e.g. for permission prompts requested from a
  // background context) and is only used if the user clicks a Web Store link.
  extensions::ExtensionInstallDialogViewAndroid::Show(
      show_params->GetParentWebContents(), std::move(prompt),
      std::move(done_callback), window_android);
}

}  // namespace

namespace extensions {

// static
void ExtensionInstallDialogViewAndroid::Show(
    content::WebContents* web_contents,
    std::unique_ptr<InstallPromptData> prompt,
    ExtensionInstallPrompt::DoneCallback done_callback,
    ui::WindowAndroid* window_android) {
  auto dialog = jni_zero::MakeUnique<ExtensionInstallDialogViewAndroid>(
      web_contents, std::move(prompt), std::move(done_callback));
  // Java only destroys the object when the dialog is dismissed.
  ExtensionInstallDialogViewAndroid* self = dialog.get();
  JNIEnv* env = base::android::AttachCurrentThread();
  self->java_object_.Reset(Java_ExtensionInstallDialogBridge_create(
      env, std::move(dialog), window_android));

  self->BuildPropertyModel();
  Java_ExtensionInstallDialogBridge_showDialog(env, self->java_object_);
}

ExtensionInstallDialogViewAndroid::ExtensionInstallDialogViewAndroid(
    content::WebContents* web_contents,
    std::unique_ptr<InstallPromptData> prompt,
    ExtensionInstallPrompt::DoneCallback done_callback)
    : web_contents_(web_contents),
      prompt_(std::move(prompt)),
      done_callback_(std::move(done_callback)) {}

ExtensionInstallDialogViewAndroid::~ExtensionInstallDialogViewAndroid() {
  if (!done_callback_) {
    return;
  }

  prompt_->OnDialogCanceled();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
}

void ExtensionInstallDialogViewAndroid::OnDialogAccepted(
    const std::string& justification_text,
    bool with_withheld_permissions) {
  prompt_->OnDialogAccepted();
  auto result =
      with_withheld_permissions
          ? ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS
          : ExtensionInstallPrompt::Result::ACCEPTED;
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(result,
                                                       justification_text));
}

void ExtensionInstallDialogViewAndroid::OnDialogCanceled() {
  OnDialogDismissed();
}

void ExtensionInstallDialogViewAndroid::OnDialogDismissed() {
  prompt_->OnDialogCanceled();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
}

void ExtensionInstallDialogViewAndroid::OnStoreLinkClicked(
    const std::string& url) {
  if (!web_contents_) {
    return;
  }
  GURL gurl(url);
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          gurl, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

void ExtensionInstallDialogViewAndroid::BuildPropertyModel() {
  JNIEnv* env = base::android::AttachCurrentThread();

  bool has_permissions = prompt_->GetPermissionCount() > 0;
  if (has_permissions) {
    std::u16string permissions_heading = prompt_->GetPermissionsHeading();
    std::u16string permissions_show_details =
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_ALL);
    std::u16string permissions_hide_details =
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_LESS);

    std::vector<std::u16string> permissions_text;
    std::vector<std::u16string> permissions_visible_details;
    std::vector<std::u16string> permissions_details;
    auto permissions = prompt_->GetPermissions();
    for (size_t i = 0; i < permissions.permissions.size(); ++i) {
      permissions_text.push_back(permissions.permissions[i]);
      permissions_visible_details.push_back(permissions.visible_details[i]);
      permissions_details.push_back(permissions.collapsed_details[i]);
    }

    Java_ExtensionInstallDialogBridge_withPermissions(
        env, java_object_, permissions_heading, permissions_text,
        permissions_visible_details, permissions_details,
        permissions_show_details, permissions_hide_details);
  }

  bool requires_justification =
      prompt_->type() == InstallPromptData::EXTENSION_REQUEST_PROMPT;
  if (requires_justification) {
    std::u16string justification_heading = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION);
    std::u16string justification_placeholder = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION_PLACEHOLDER);

    Java_ExtensionInstallDialogBridge_withJustification(
        env, java_object_, justification_heading, justification_placeholder);
  }

  if (prompt_->ShouldWithheldPermissionsOnDialogAccept()) {
    std::u16string site_access_heading =
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_ALLOW_SITE_ACCESS_TITLE);
    std::u16string on_click_text = l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK);
    std::u16string always_all_sites_text = l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_ALL_SITES_V2);

    Java_ExtensionInstallDialogBridge_withSiteAccessOptions(
        env, java_object_, site_access_heading, on_click_text,
        always_all_sites_text);
  }

  if (prompt_->has_webstore_data()) {
    std::u16string store_link_text =
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK);
    std::u16string rating_count_text = prompt_->GetRatingCount();
    std::u16string user_count_text = prompt_->GetUserCount();
    double average_rating = prompt_->average_rating();
    std::string store_url = extension_urls::GetWebstoreItemDetailURLPrefix() +
                            prompt_->extension()->id();

    Java_ExtensionInstallDialogBridge_withWebstoreData(
        env, java_object_, store_link_text, rating_count_text, user_count_text,
        average_rating, store_url);
  }

  Java_ExtensionInstallDialogBridge_buildDialog(
      env, java_object_, prompt_->GetDialogTitle(), prompt_->icon().AsBitmap(),
      prompt_->GetAcceptButtonLabel(), prompt_->GetAbortButtonLabel());
}

}  // namespace extensions

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::BindRepeating(&ShowExtensionInstallDialogAndroid);
}

DEFINE_JNI(ExtensionInstallDialogBridge)
