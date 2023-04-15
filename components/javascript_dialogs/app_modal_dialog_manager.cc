// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_manager.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/javascript_dialogs/app_modal_dialog_manager_delegate.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/javascript_dialogs/extensions_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/javascript_dialog_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "url/origin.h"

namespace javascript_dialogs {

namespace {

class DefaultExtensionsClient : public ExtensionsClient {
 public:
  DefaultExtensionsClient() = default;

  DefaultExtensionsClient(const DefaultExtensionsClient&) = delete;
  DefaultExtensionsClient& operator=(const DefaultExtensionsClient&) = delete;

  ~DefaultExtensionsClient() override = default;

 private:
  // ExtensionsClient:
  void OnDialogOpened(content::WebContents* web_contents) override {}
  void OnDialogClosed(content::WebContents* web_contents) override {}
};

bool ShouldDisplaySuppressCheckbox(
    ChromeJavaScriptDialogExtraData* extra_data) {
  return extra_data->has_already_shown_a_dialog_;
}

}  // namespace

// static
AppModalDialogManager* AppModalDialogManager::GetInstance() {
  return base::Singleton<AppModalDialogManager>::get();
}

void AppModalDialogManager::SetNativeDialogFactory(
    AppModalViewFactory factory) {
  view_factory_ = std::move(factory);
}

void AppModalDialogManager::SetExtensionsClient(
    std::unique_ptr<ExtensionsClient> extensions_client) {
  extensions_client_ = std::move(extensions_client);
}

void AppModalDialogManager::SetDelegate(
    std::unique_ptr<AppModalDialogManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
}

AppModalDialogManager::AppModalDialogManager()
    : extensions_client_(new DefaultExtensionsClient) {}

AppModalDialogManager::~AppModalDialogManager() = default;

std::u16string AppModalDialogManager::GetTitle(
    content::WebContents* web_contents,
    const url::Origin& alerting_frame_origin) {
  if (delegate_) {
    return delegate_->GetTitle(web_contents, alerting_frame_origin);
  }

  // Otherwise, return the formatted URL.
  return GetSiteFrameTitle(
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      alerting_frame_origin);
}

namespace {

// If an origin is opaque but has a precursor, then returns the precursor
// origin. If the origin is not opaque, returns it unchanged. Unwrapping origins
// allows the dialog code to provide the user with a clearer picture of which
// page is actually showing the dialog.
url::Origin UnwrapOriginIfOpaque(const url::Origin& origin) {
  if (!origin.opaque())
    return origin;

  const url::SchemeHostPort& precursor =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  if (!precursor.IsValid())
    return origin;

  return url::Origin::CreateFromNormalizedTuple(
      precursor.scheme(), precursor.host(), precursor.port());
}

}  // namespace

// static
std::u16string AppModalDialogManager::GetSiteFrameTitle(
    const url::Origin& main_frame_origin,
    const url::Origin& alerting_frame_origin) {
  // Note that `Origin::Create()` handles unwrapping of `blob:` and
  // `filesystem:` schemed URLs, so no special handling is needed for that.
  // However, origins can be opaque but have precursors that are origins that a
  // user would be able to make sense of, so do unwrapping for that.
  const url::Origin unwrapped_main_frame_origin =
      UnwrapOriginIfOpaque(main_frame_origin);
  const url::Origin unwrapped_alerting_frame_origin =
      UnwrapOriginIfOpaque(alerting_frame_origin);

  bool is_same_origin_as_main_frame =
      unwrapped_alerting_frame_origin.IsSameOriginWith(
          unwrapped_main_frame_origin);
  if (unwrapped_alerting_frame_origin.GetURL().IsStandard() &&
      !unwrapped_alerting_frame_origin.GetURL().SchemeIsFile()) {
    std::u16string origin_string =
        url_formatter::FormatOriginForSecurityDisplay(
            unwrapped_alerting_frame_origin,
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    return l10n_util::GetStringFUTF16(
        is_same_origin_as_main_frame ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE
                                     : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_IFRAME,
        base::i18n::GetDisplayStringInLTRDirectionality(origin_string));
  }
  return l10n_util::GetStringUTF16(
      is_same_origin_as_main_frame
          ? IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL
          : IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
}

void AppModalDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  *did_suppress_message = false;

  ChromeJavaScriptDialogExtraData* extra_data =
      &javascript_dialog_extra_data_[web_contents];

  if (extra_data->suppress_javascript_messages_) {
    *did_suppress_message = true;
    return;
  }

  std::u16string dialog_title =
      GetTitle(web_contents, render_frame_host->GetLastCommittedOrigin());

  extensions_client_->OnDialogOpened(web_contents);

  AppModalDialogQueue::GetInstance()->AddDialog(new AppModalDialogController(
      web_contents, &javascript_dialog_extra_data_, dialog_title, dialog_type,
      message_text, default_prompt_text,
      ShouldDisplaySuppressCheckbox(extra_data),
      false,  // is_before_unload_dialog
      false,  // is_reload
      base::BindOnce(&AppModalDialogManager::OnDialogClosed,
                     base::Unretained(this), web_contents,
                     std::move(callback))));
}

void AppModalDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  RunBeforeUnloadDialogWithOptions(web_contents, render_frame_host, is_reload,
                                   false, std::move(callback));
}

void AppModalDialogManager::RunBeforeUnloadDialogWithOptions(
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
    std::move(callback).Run(true, std::u16string());
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

  std::u16string title;
  if (is_app) {
    title = l10n_util::GetStringUTF16(
        is_reload ? IDS_BEFORERELOAD_APP_MESSAGEBOX_TITLE
                  : IDS_BEFOREUNLOAD_APP_MESSAGEBOX_TITLE);
  } else {
    title = l10n_util::GetStringUTF16(is_reload
                                          ? IDS_BEFORERELOAD_MESSAGEBOX_TITLE
                                          : IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE);
  }
  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_MESSAGE);

  extensions_client_->OnDialogOpened(web_contents);

  AppModalDialogQueue::GetInstance()->AddDialog(new AppModalDialogController(
      web_contents, &javascript_dialog_extra_data_, title,
      content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, message,
      std::u16string(),  // default_prompt_text
      ShouldDisplaySuppressCheckbox(extra_data),
      true,  // is_before_unload_dialog
      is_reload,
      base::BindOnce(&AppModalDialogManager::OnDialogClosed,
                     base::Unretained(this), web_contents,
                     std::move(callback))));
}

bool AppModalDialogManager::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  if (!dialog_queue->HasActiveDialog() ||
      dialog_queue->active_dialog()->web_contents() != web_contents) {
    return false;
  }

  AppModalDialogController* dialog =
      static_cast<AppModalDialogController*>(dialog_queue->active_dialog());

  if (dialog->javascript_dialog_type() ==
      content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_ALERT) {
    // Alert dialogs only have one button: OK. Any "handling" of this dialog has
    // to be a click on the OK button.
    accept = true;
  }

  if (accept) {
    if (prompt_override)
      dialog->SetOverridePromptText(*prompt_override);
    dialog->view()->AcceptAppModalDialog();
  } else {
    dialog->view()->CancelAppModalDialog();
  }
  return true;
}

void AppModalDialogManager::CancelDialogs(content::WebContents* web_contents,
                                          bool reset_state) {
  AppModalDialogQueue* queue = AppModalDialogQueue::GetInstance();
  for (auto* dialog : *queue) {
    if (dialog->web_contents() == web_contents)
      dialog->Invalidate();
  }
  AppModalDialogController* active_dialog = queue->active_dialog();
  if (active_dialog && active_dialog->web_contents() == web_contents)
    active_dialog->Invalidate();

  if (reset_state)
    javascript_dialog_extra_data_.erase(web_contents);
}

void AppModalDialogManager::OnDialogClosed(content::WebContents* web_contents,
                                           DialogClosedCallback callback,
                                           bool success,
                                           const std::u16string& user_input) {
  // If an extension opened this dialog then the extension may shut down its
  // lazy background page after the dialog closes. (Dialogs are closed before
  // their WebContents is destroyed so |web_contents| is still valid here.)
  extensions_client_->OnDialogClosed(web_contents);
  std::move(callback).Run(success, user_input);
}

}  // namespace javascript_dialogs
