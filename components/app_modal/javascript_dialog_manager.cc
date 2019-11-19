// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_modal/javascript_dialog_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_dialog_extensions_client.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/javascript_dialog_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"

namespace app_modal {

namespace {

#if !defined(OS_ANDROID)
// Keep in sync with kDefaultMessageWidth, but allow some space for the rest of
// the text.
const int kUrlElideWidth = 350;
#endif

class DefaultExtensionsClient : public JavaScriptDialogExtensionsClient {
 public:
  DefaultExtensionsClient() {}
  ~DefaultExtensionsClient() override {}

 private:
  // JavaScriptDialogExtensionsClient:
  void OnDialogOpened(content::WebContents* web_contents) override {}
  void OnDialogClosed(content::WebContents* web_contents) override {}
  bool GetExtensionName(content::WebContents* web_contents,
                        const GURL& alerting_frame_url,
                        std::string* name_out) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultExtensionsClient);
};

bool ShouldDisplaySuppressCheckbox(
    ChromeJavaScriptDialogExtraData* extra_data) {
  return extra_data->has_already_shown_a_dialog_;
}

}  // namespace

// static
JavaScriptDialogManager* JavaScriptDialogManager::GetInstance() {
  return base::Singleton<JavaScriptDialogManager>::get();
}

void JavaScriptDialogManager::SetNativeDialogFactory(
    std::unique_ptr<JavaScriptNativeDialogFactory> factory) {
  native_dialog_factory_ = std::move(factory);
}

void JavaScriptDialogManager::SetExtensionsClient(
    std::unique_ptr<JavaScriptDialogExtensionsClient> extensions_client) {
  extensions_client_ = std::move(extensions_client);
}

JavaScriptDialogManager::JavaScriptDialogManager()
    : extensions_client_(new DefaultExtensionsClient) {
}

JavaScriptDialogManager::~JavaScriptDialogManager() {
}

base::string16 JavaScriptDialogManager::GetTitle(
    content::WebContents* web_contents,
    const GURL& alerting_frame_url) {
  // For extensions, show the extension name, but only if the origin of
  // the alert matches the top-level WebContents.
  std::string name;
  if (extensions_client_->GetExtensionName(web_contents, alerting_frame_url,
                                           &name))
    return base::UTF8ToUTF16(name);

  // Otherwise, return the formatted URL.
  return GetTitleImpl(web_contents->GetURL(), alerting_frame_url);
}

namespace {

// Unwraps an URL to get to an embedded URL.
GURL UnwrapURL(const GURL& url) {
  // GURL will unwrap filesystem:// URLs so ask it to do so.
  const GURL* unwrapped_url = url.inner_url();
  if (unwrapped_url)
    return *unwrapped_url;

  // GURL::inner_url() should unwrap blob: URLs but doesn't do so
  // (https://crbug.com/690091). Therefore, do it manually.
  //
  // https://url.spec.whatwg.org/#origin defines the origin of a blob:// URL as
  // the origin of the URL which results from parsing the "path", which boils
  // down to everything after the scheme. GURL's 'GetContent()' gives us exactly
  // that. See url::Origin()'s constructor.
  if (url.SchemeIsBlob())
    return GURL(url.GetContent());

  return url;
}

}  // namespace

// static
base::string16 JavaScriptDialogManager::GetTitleImpl(
    const GURL& parent_frame_url,
    const GURL& alerting_frame_url) {
  GURL unwrapped_parent_frame_url = UnwrapURL(parent_frame_url);
  GURL unwrapped_alerting_frame_url = UnwrapURL(alerting_frame_url);

  bool is_same_origin_as_main_frame =
      (unwrapped_parent_frame_url.GetOrigin() ==
       unwrapped_alerting_frame_url.GetOrigin());
  if (unwrapped_alerting_frame_url.IsStandard() &&
      !unwrapped_alerting_frame_url.SchemeIsFile()) {
#if defined(OS_ANDROID)
    base::string16 url_string = url_formatter::FormatUrlForSecurityDisplay(
        unwrapped_alerting_frame_url,
        url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
#else
    base::string16 url_string = url_formatter::ElideHost(
        unwrapped_alerting_frame_url, gfx::FontList(), kUrlElideWidth);
#endif
    return l10n_util::GetStringFUTF16(
        is_same_origin_as_main_frame ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE
                                     : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_IFRAME,
        base::i18n::GetDisplayStringInLTRDirectionality(url_string));
  }
  return l10n_util::GetStringUTF16(
      is_same_origin_as_main_frame
          ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL
          : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
}

void JavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  *did_suppress_message = false;

  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_[web_contents];

  if (extra_data->suppress_javascript_messages_) {
    *did_suppress_message = true;
    return;
  }

  base::string16 dialog_title =
      GetTitle(web_contents, render_frame_host->GetLastCommittedURL());

  extensions_client_->OnDialogOpened(web_contents);

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      web_contents, &javascript_dialog_extra_data_, dialog_title, dialog_type,
      message_text, default_prompt_text,
      ShouldDisplaySuppressCheckbox(extra_data),
      false,  // is_before_unload_dialog
      false,  // is_reload
      base::BindOnce(&JavaScriptDialogManager::OnDialogClosed,
                     base::Unretained(this), web_contents,
                     std::move(callback))));
}

void JavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  RunBeforeUnloadDialogWithOptions(web_contents, render_frame_host, is_reload,
                                   false, std::move(callback));
}

void JavaScriptDialogManager::RunBeforeUnloadDialogWithOptions(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    bool is_app,
    DialogClosedCallback callback) {
  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_[web_contents];

  if (extra_data->suppress_javascript_messages_) {
    // If a site harassed the user enough for them to put it on mute, then it
    // lost its privilege to deny unloading.
    std::move(callback).Run(true, base::string16());
    return;
  }

  // Build the dialog message. We explicitly do _not_ allow the webpage to
  // specify the contents of this dialog, as per the current spec
  //
  // https://html.spec.whatwg.org/#unloading-documents, step 8:
  //
  // "The message shown to the user is not customizable, but instead
  // determined by the user agent. In particular, the actual value of the
  // returnValue attribute is ignored."
  //
  // This message used to be customizable, but it was frequently abused by
  // scam websites so the specification was changed.

  base::string16 title;
  if (is_app) {
    title = l10n_util::GetStringUTF16(
        is_reload ? IDS_BEFORERELOAD_APP_MESSAGEBOX_TITLE
                  : IDS_BEFOREUNLOAD_APP_MESSAGEBOX_TITLE);
  } else {
    title = l10n_util::GetStringUTF16(is_reload
                                          ? IDS_BEFORERELOAD_MESSAGEBOX_TITLE
                                          : IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE);
  }
  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_MESSAGE);

  extensions_client_->OnDialogOpened(web_contents);

  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      web_contents, &javascript_dialog_extra_data_, title,
      content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, message,
      base::string16(),  // default_prompt_text
      ShouldDisplaySuppressCheckbox(extra_data),
      true,  // is_before_unload_dialog
      is_reload,
      base::BindOnce(&JavaScriptDialogManager::OnBeforeUnloadDialogClosed,
                     base::Unretained(this), web_contents,
                     std::move(callback))));
}

bool JavaScriptDialogManager::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const base::string16* prompt_override) {
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  if (!dialog_queue->HasActiveDialog() ||
      dialog_queue->active_dialog()->web_contents() != web_contents) {
    return false;
  }

  JavaScriptAppModalDialog* dialog = static_cast<JavaScriptAppModalDialog*>(
      dialog_queue->active_dialog());

  if (dialog->javascript_dialog_type() ==
      content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_ALERT) {
    // Alert dialogs only have one button: OK. Any "handling" of this dialog has
    // to be a click on the OK button.
    accept = true;
  }

  if (accept) {
    if (prompt_override)
      dialog->SetOverridePromptText(*prompt_override);
    dialog->native_dialog()->AcceptAppModalDialog();
  } else {
    dialog->native_dialog()->CancelAppModalDialog();
  }
  return true;
}

void JavaScriptDialogManager::CancelDialogs(content::WebContents* web_contents,
                                            bool reset_state) {
  AppModalDialogQueue* queue = AppModalDialogQueue::GetInstance();
  JavaScriptAppModalDialog* active_dialog = queue->active_dialog();
  for (auto* dialog : *queue) {
    // Invalidating the active dialog might trigger showing a not-yet
    // invalidated dialog, so invalidate the active dialog last.
    if (dialog == active_dialog)
      continue;
    if (dialog->web_contents() == web_contents)
      dialog->Invalidate();
  }
  if (active_dialog && active_dialog->web_contents() == web_contents)
    active_dialog->Invalidate();

  if (reset_state)
    javascript_dialog_extra_data_.erase(web_contents);
}

void JavaScriptDialogManager::OnBeforeUnloadDialogClosed(
    content::WebContents* web_contents,
    DialogClosedCallback callback,
    bool success,
    const base::string16& user_input) {
  enum class StayVsLeave {
    STAY = 0,
    LEAVE = 1,
    MAX,
  };
  UMA_HISTOGRAM_ENUMERATION(
      "JSDialogs.OnBeforeUnloadStayVsLeave",
      static_cast<int>(success ? StayVsLeave::LEAVE : StayVsLeave::STAY),
      static_cast<int>(StayVsLeave::MAX));

  OnDialogClosed(web_contents, std::move(callback), success, user_input);
}

void JavaScriptDialogManager::OnDialogClosed(
    content::WebContents* web_contents,
    DialogClosedCallback callback,
    bool success,
    const base::string16& user_input) {
  // If an extension opened this dialog then the extension may shut down its
  // lazy background page after the dialog closes. (Dialogs are closed before
  // their WebContents is destroyed so |web_contents| is still valid here.)
  extensions_client_->OnDialogClosed(web_contents);
  std::move(callback).Run(success, user_input);
}

}  // namespace app_modal
