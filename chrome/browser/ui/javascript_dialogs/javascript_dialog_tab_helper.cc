// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/gfx/text_elider.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/javascript_dialog_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

app_modal::JavaScriptDialogManager* AppModalDialogManager() {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

bool IsWebContentsForemost(content::WebContents* web_contents) {
#if defined(OS_ANDROID)
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  if (tab_model) {
    return tab_model->IsCurrentModel() &&
           tab_model->GetActiveWebContents() == web_contents;
  } else {
    // If tab model is not found (e.g. single tab model), fall back to check
    // whether the tab for this web content is interactive.
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    return tab && tab->IsUserInteractable();
  }
#else
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
#endif
}

// The relationship between origins in displayed dialogs.
//
// This is used for a UMA histogram. Please never alter existing values, only
// append new ones.
//
// Note that "HTTP" in these enum names refers to a scheme that is either HTTP
// or HTTPS.
enum class DialogOriginRelationship {
  // The dialog was shown by a main frame with a non-HTTP(S) scheme, or by a
  // frame within a non-HTTP(S) main frame.
  NON_HTTP_MAIN_FRAME = 1,

  // The dialog was shown by a main frame with an HTTP(S) scheme.
  HTTP_MAIN_FRAME = 2,

  // The dialog was displayed by an HTTP(S) frame which shared the same origin
  // as the main frame.
  HTTP_MAIN_FRAME_HTTP_SAME_ORIGIN_ALERTING_FRAME = 3,

  // The dialog was displayed by an HTTP(S) frame which had a different origin
  // from the main frame.
  HTTP_MAIN_FRAME_HTTP_DIFFERENT_ORIGIN_ALERTING_FRAME = 4,

  // The dialog was displayed by a non-HTTP(S) frame whose nearest HTTP(S)
  // ancestor shared the same origin as the main frame.
  HTTP_MAIN_FRAME_NON_HTTP_ALERTING_FRAME_SAME_ORIGIN_ANCESTOR = 5,

  // The dialog was displayed by a non-HTTP(S) frame whose nearest HTTP(S)
  // ancestor was a different origin than the main frame.
  HTTP_MAIN_FRAME_NON_HTTP_ALERTING_FRAME_DIFFERENT_ORIGIN_ANCESTOR = 6,

  COUNT,
};

DialogOriginRelationship GetDialogOriginRelationship(
    content::WebContents* web_contents,
    content::RenderFrameHost* alerting_frame) {
  GURL main_frame_url = web_contents->GetLastCommittedURL();

  if (!main_frame_url.SchemeIsHTTPOrHTTPS())
    return DialogOriginRelationship::NON_HTTP_MAIN_FRAME;

  if (alerting_frame == web_contents->GetMainFrame())
    return DialogOriginRelationship::HTTP_MAIN_FRAME;

  GURL alerting_frame_url = alerting_frame->GetLastCommittedURL();

  if (alerting_frame_url.SchemeIsHTTPOrHTTPS()) {
    if (main_frame_url.GetOrigin() == alerting_frame_url.GetOrigin()) {
      return DialogOriginRelationship::
          HTTP_MAIN_FRAME_HTTP_SAME_ORIGIN_ALERTING_FRAME;
    }
    return DialogOriginRelationship::
        HTTP_MAIN_FRAME_HTTP_DIFFERENT_ORIGIN_ALERTING_FRAME;
  }

  // Walk up the tree to find the nearest ancestor frame of the alerting frame
  // that has an HTTP(S) scheme. Note that this is guaranteed to terminate
  // because the main frame has an HTTP(S) scheme.
  content::RenderFrameHost* nearest_http_ancestor_frame =
      alerting_frame->GetParent();
  while (!nearest_http_ancestor_frame->GetLastCommittedURL()
              .SchemeIsHTTPOrHTTPS()) {
    nearest_http_ancestor_frame = nearest_http_ancestor_frame->GetParent();
  }

  GURL nearest_http_ancestor_frame_url =
      nearest_http_ancestor_frame->GetLastCommittedURL();

  if (main_frame_url.GetOrigin() ==
      nearest_http_ancestor_frame_url.GetOrigin()) {
    return DialogOriginRelationship::
        HTTP_MAIN_FRAME_NON_HTTP_ALERTING_FRAME_SAME_ORIGIN_ANCESTOR;
  }
  return DialogOriginRelationship::
      HTTP_MAIN_FRAME_NON_HTTP_ALERTING_FRAME_DIFFERENT_ORIGIN_ANCESTOR;
}

}  // namespace

JavaScriptDialogTabHelper::JavaScriptDialogTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

JavaScriptDialogTabHelper::~JavaScriptDialogTabHelper() {
#if !defined(OS_ANDROID)
  DCHECK(!tab_strip_model_being_observed_);
#endif
  CloseDialog(DismissalCause::kTabHelperDestroyed, false, base::string16());
}

void JavaScriptDialogTabHelper::SetDialogShownCallbackForTesting(
    base::OnceClosure callback) {
  dialog_shown_ = std::move(callback);
}

bool JavaScriptDialogTabHelper::IsShowingDialogForTesting() const {
  return !!dialog_;
}

void JavaScriptDialogTabHelper::ClickDialogButtonForTesting(
    bool accept,
    const base::string16& user_input) {
  DCHECK(!!dialog_);
  CloseDialog(DismissalCause::kDialogButtonClicked, accept, user_input);
}

void JavaScriptDialogTabHelper::RunJavaScriptDialog(
    content::WebContents* alerting_web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  DCHECK_EQ(alerting_web_contents,
            content::WebContents::FromRenderFrameHost(render_frame_host));

  GURL alerting_frame_url = render_frame_host->GetLastCommittedURL();

  content::WebContents* parent_web_contents =
      WebContentsObserver::web_contents();
  DialogOriginRelationship origin_relationship =
      GetDialogOriginRelationship(alerting_web_contents, render_frame_host);
  bool foremost = IsWebContentsForemost(parent_web_contents);
  navigation_metrics::Scheme scheme =
      navigation_metrics::GetScheme(alerting_frame_url);
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.OriginRelationship.Alert",
                                origin_relationship,
                                DialogOriginRelationship::COUNT);
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Alert", foremost);
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.Alert", scheme,
                                navigation_metrics::Scheme::COUNT);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.OriginRelationship.Confirm",
                                origin_relationship,
                                DialogOriginRelationship::COUNT);
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Confirm", foremost);
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.Confirm", scheme,
                                navigation_metrics::Scheme::COUNT);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.OriginRelationship.Prompt",
                                origin_relationship,
                                DialogOriginRelationship::COUNT);
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Prompt", foremost);
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.Prompt", scheme,
                                navigation_metrics::Scheme::COUNT);
      break;
  }

  // Close any dialog already showing.
  CloseDialog(DismissalCause::kSubsequentDialogShown, false, base::string16());

  bool make_pending = false;
  if (!IsWebContentsForemost(parent_web_contents) &&
      !content::DevToolsAgentHost::IsDebuggerAttached(parent_web_contents)) {
    static const char kDialogSuppressedConsoleMessageFormat[] =
        "A window.%s() dialog generated by this page was suppressed "
        "because this page is not the active tab of the front window. "
        "Please make sure your dialogs are triggered by user interactions "
        "to avoid this situation. https://www.chromestatus.com/feature/%s";

    switch (dialog_type) {
      case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
        // When an alert fires in the background, make the callback so that the
        // render process can continue.
        std::move(callback).Run(true, base::string16());
        callback.Reset();

        SetTabNeedsAttention(true);

        make_pending = true;
        break;
      }
      case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
        *did_suppress_message = true;
        alerting_web_contents->GetMainFrame()->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            base::StringPrintf(kDialogSuppressedConsoleMessageFormat, "confirm",
                               "5140698722467840"));
        return;
      }
      case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
        *did_suppress_message = true;
        alerting_web_contents->GetMainFrame()->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            base::StringPrintf(kDialogSuppressedConsoleMessageFormat, "prompt",
                               "5637107137642496"));
        return;
      }
    }
  }

  // Enforce sane sizes. ElideRectangleString breaks horizontally, which isn't
  // strictly needed, but it restricts the vertical size, which is crucial.
  // This gives about 2000 characters, which is about the same as the
  // AppModalDialogManager provides, but allows no more than 24 lines.
  const int kMessageTextMaxRows = 24;
  const int kMessageTextMaxCols = 80;
  const size_t kDefaultPromptMaxSize = 2000;
  base::string16 truncated_message_text;
  gfx::ElideRectangleString(message_text, kMessageTextMaxRows,
                            kMessageTextMaxCols, false,
                            &truncated_message_text);
  base::string16 truncated_default_prompt_text;
  gfx::ElideString(default_prompt_text, kDefaultPromptMaxSize,
                   &truncated_default_prompt_text);

  base::string16 title = AppModalDialogManager()->GetTitle(
      alerting_web_contents, alerting_frame_url);
  dialog_callback_ = std::move(callback);
  dialog_type_ = dialog_type;
  if (make_pending) {
    DCHECK(!dialog_);
    pending_dialog_ = base::BindOnce(
        &JavaScriptDialog::CreateNewDialog, parent_web_contents,
        alerting_web_contents, title, dialog_type, truncated_message_text,
        truncated_default_prompt_text,
        base::BindOnce(&JavaScriptDialogTabHelper::CloseDialog,
                       base::Unretained(this),
                       DismissalCause::kDialogButtonClicked),
        base::BindOnce(&JavaScriptDialogTabHelper::CloseDialog,
                       base::Unretained(this), DismissalCause::kDialogClosed,
                       false, base::string16()));
  } else {
    DCHECK(!pending_dialog_);
    dialog_ = JavaScriptDialog::CreateNewDialog(
        parent_web_contents, alerting_web_contents, title, dialog_type,
        truncated_message_text, truncated_default_prompt_text,
        base::BindOnce(&JavaScriptDialogTabHelper::CloseDialog,
                       base::Unretained(this),
                       DismissalCause::kDialogButtonClicked),
        base::BindOnce(&JavaScriptDialogTabHelper::CloseDialog,
                       base::Unretained(this), DismissalCause::kDialogClosed,
                       false, base::string16()));
  }

#if !defined(OS_ANDROID)
  BrowserList::AddObserver(this);
#endif

  // Message suppression is something that we don't give the user a checkbox
  // for any more. It was useful back in the day when dialogs were app-modal
  // and clicking the checkbox was the only way to escape a loop that the page
  // was doing, but now the user can just close the page.
  *did_suppress_message = false;

  if (!dialog_shown_.is_null())
    std::move(dialog_shown_).Run();
}

void JavaScriptDialogTabHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  DCHECK_EQ(web_contents,
            content::WebContents::FromRenderFrameHost(render_frame_host));

  content::WebContents* parent_web_contents =
      WebContentsObserver::web_contents();
  DialogOriginRelationship origin_relationship =
      GetDialogOriginRelationship(web_contents, render_frame_host);
  bool foremost = IsWebContentsForemost(parent_web_contents);
  navigation_metrics::Scheme scheme =
      navigation_metrics::GetScheme(render_frame_host->GetLastCommittedURL());
  UMA_HISTOGRAM_ENUMERATION("JSDialogs.OriginRelationship.BeforeUnload",
                            origin_relationship,
                            DialogOriginRelationship::COUNT);
  UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.BeforeUnload", foremost);
  UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.BeforeUnload", scheme,
                            navigation_metrics::Scheme::COUNT);

  // onbeforeunload dialogs are always handled with an app-modal dialog, because
  // - they are critical to the user not losing data
  // - they can be requested for tabs that are not foremost
  // - they can be requested for many tabs at the same time
  // and therefore auto-dismissal is inappropriate for them.

  bool browser_is_app = false;
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    browser_is_app = browser->deprecated_is_app();
  }
#endif
  return AppModalDialogManager()->RunBeforeUnloadDialogWithOptions(
      web_contents, render_frame_host, is_reload, browser_is_app,
      std::move(callback));
}

bool JavaScriptDialogTabHelper::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const base::string16* prompt_override) {
  if (dialog_ || pending_dialog_) {
    CloseDialog(DismissalCause::kHandleDialogCalled, accept,
                prompt_override ? *prompt_override : dialog_->GetUserInput());
    return true;
  }

  // Handle any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->HandleJavaScriptDialog(web_contents, accept,
                                                         prompt_override);
}

void JavaScriptDialogTabHelper::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  CloseDialog(DismissalCause::kCancelDialogsCalled, false, base::string16());

  // Cancel any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->CancelDialogs(web_contents, reset_state);
}

void JavaScriptDialogTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    HandleTabSwitchAway(DismissalCause::kTabHidden);
  } else if (pending_dialog_) {
    dialog_ = std::move(pending_dialog_).Run();
    pending_dialog_.Reset();
    SetTabNeedsAttention(false);
  }
}

void JavaScriptDialogTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Close the dialog if the user started a new navigation. This allows reloads
  // and history navigations to proceed.
  CloseDialog(DismissalCause::kTabNavigated, false, base::string16());
}

#if !defined(OS_ANDROID)
void JavaScriptDialogTabHelper::OnBrowserSetLastActive(Browser* browser) {
  if (IsWebContentsForemost(web_contents())) {
    OnVisibilityChanged(content::Visibility::VISIBLE);
  } else {
    HandleTabSwitchAway(DismissalCause::kBrowserSwitched);
  }
}

void JavaScriptDialogTabHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    if (replace->old_contents == WebContentsObserver::web_contents()) {
      // At this point, this WebContents is no longer in the tabstrip. The usual
      // teardown will not be able to turn off the attention indicator, so that
      // must be done here.
      SetTabNeedsAttentionImpl(false, tab_strip_model, replace->index);

      CloseDialog(DismissalCause::kTabSwitchedOut, false, base::string16());
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      if (contents.contents == WebContentsObserver::web_contents()) {
        // We don't call TabStripModel::SetTabNeedsAttention because it causes
        // re-entrancy into TabStripModel and correctness of the |index|
        // parameter is dependent on observer ordering. This is okay in the
        // short term because the tab in question is being removed.
        // TODO(erikchen): Clean up TabStripModel observer API so that this
        // doesn't require re-entrancy and/or works correctly
        // https://crbug.com/842194.
        DCHECK(tab_strip_model_being_observed_);
        tab_strip_model_being_observed_->RemoveObserver(this);
        tab_strip_model_being_observed_ = nullptr;
        CloseDialog(DismissalCause::kTabHelperDestroyed, false,
                    base::string16());
        break;
      }
    }
  }
}
#endif

void JavaScriptDialogTabHelper::LogDialogDismissalCause(DismissalCause cause) {
  // Log to UMA.
  switch (dialog_type_) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Alert", cause);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Confirm", cause);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Prompt", cause);
      break;
  }

  // Log to UKM.
  //
  // Note that this will return the outermost WebContents, not necessarily the
  // WebContents that had the alert call in it. For 99.9999% of cases they're
  // the same, but for instances like the <webview> tag in extensions and PDF
  // files that alert they may differ.
  ukm::SourceId source_id = ukm::GetSourceIdForWebContentsDocument(
      WebContentsObserver::web_contents());
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::AbusiveExperienceHeuristic_JavaScriptDialog(source_id)
        .SetDismissalCause(static_cast<int64_t>(cause))
        .Record(ukm::UkmRecorder::Get());
  }
}

void JavaScriptDialogTabHelper::HandleTabSwitchAway(DismissalCause cause) {
  if (!dialog_ || content::DevToolsAgentHost::IsDebuggerAttached(
                      WebContentsObserver::web_contents())) {
    return;
  }

  if (dialog_type_ == content::JAVASCRIPT_DIALOG_TYPE_ALERT) {
    // When the user switches tabs, make the callback so that the render process
    // can continue.
    if (dialog_callback_) {
      std::move(dialog_callback_).Run(true, base::string16());
      dialog_callback_.Reset();
    }
  } else {
    CloseDialog(cause, false, base::string16());
  }
}

void JavaScriptDialogTabHelper::CloseDialog(DismissalCause cause,
                                            bool success,
                                            const base::string16& user_input) {
  if (!dialog_ && !pending_dialog_)
    return;

  LogDialogDismissalCause(cause);

  // CloseDialog() can be called two ways. It can be called from within
  // JavaScriptDialogTabHelper, in which case the dialog needs to be closed.
  // However, it can also be called, bound, from the JavaScriptDialog. In that
  // case, the dialog is already closing, so the JavaScriptDialog doesn't need
  // to be told to close.
  //
  // Using the |cause| to distinguish a call from JavaScriptDialog vs from
  // within JavaScriptDialogTabHelper is a bit hacky, but is the simplest way.
  if (dialog_ && cause != DismissalCause::kDialogButtonClicked &&
      cause != DismissalCause::kDialogClosed)
    dialog_->CloseDialogWithoutCallback();

  // If there is a callback, call it. There might not be one, if a tab-modal
  // alert() dialog is showing.
  if (dialog_callback_)
    std::move(dialog_callback_).Run(success, user_input);

  // If there's a pending dialog, then the tab is still in the "needs attention"
  // state; clear it out. However, if the tab was switched out, the turning off
  // of the "needs attention" state was done in OnTabStripModelChanged()
  // SetTabNeedsAttention won't work, so don't call it.
  if (pending_dialog_ && cause != DismissalCause::kTabSwitchedOut &&
      cause != DismissalCause::kTabHelperDestroyed) {
    SetTabNeedsAttention(false);
  }

  dialog_.reset();
  pending_dialog_.Reset();
  dialog_callback_.Reset();

#if !defined(OS_ANDROID)
  BrowserList::RemoveObserver(this);
#endif
}

void JavaScriptDialogTabHelper::SetTabNeedsAttention(bool attention) {
#if !defined(OS_ANDROID)
  content::WebContents* web_contents = WebContentsObserver::web_contents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    // It's possible that the WebContents is no longer in the tab strip. If so,
    // just give up. https://crbug.com/786178#c7.
    return;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  SetTabNeedsAttentionImpl(
      attention, tab_strip_model,
      tab_strip_model->GetIndexOfWebContents(web_contents));
#endif
}

#if !defined(OS_ANDROID)
void JavaScriptDialogTabHelper::SetTabNeedsAttentionImpl(
    bool attention,
    TabStripModel* tab_strip_model,
    int index) {
  tab_strip_model->SetTabNeedsAttentionAt(index, attention);
  if (attention) {
    tab_strip_model->AddObserver(this);
    tab_strip_model_being_observed_ = tab_strip_model;
  } else {
    DCHECK_EQ(tab_strip_model_being_observed_, tab_strip_model);
    tab_strip_model_being_observed_->RemoveObserver(this);
    tab_strip_model_being_observed_ = nullptr;
  }
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(JavaScriptDialogTabHelper)
