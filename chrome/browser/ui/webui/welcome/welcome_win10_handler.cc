// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/welcome_win10_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

namespace {

void RecordDefaultBrowserResult(
    const std::string& histogram_suffix,
    shell_integration::DefaultWebClientState default_browser_state) {
  // Consider the page successful if this or any other side-by-side install of
  // Chrome was chosen as the user's default browser.
  base::UmaHistogramBoolean(
      base::StringPrintf("Welcome.Win10.DefaultPromptResult_%s",
                         histogram_suffix.c_str()),
      (default_browser_state == shell_integration::IS_DEFAULT ||
       default_browser_state == shell_integration::OTHER_MODE_IS_DEFAULT));
}

void RecordPinnedResult(const std::string& histogram_suffix,
                        bool succeeded,
                        bool is_pinned) {
  if (!succeeded)
    return;

  base::UmaHistogramBoolean(
      base::StringPrintf("Welcome.Win10.PinnedPromptResult_%s",
                         histogram_suffix.c_str()),
      is_pinned);
}

// Returns a new Connector that can be used on a different thread.
std::unique_ptr<service_manager::Connector> GetClonedConnector() {
  return content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->Clone();
}

}  // namespace

WelcomeWin10Handler::WelcomeWin10Handler() : weak_ptr_factory_(this) {
  // The check is started as early as possible because waiting for the page to
  // be fully loaded is unnecessarily wasting time.
  StartIsPinnedToTaskbarCheck();
}

WelcomeWin10Handler::~WelcomeWin10Handler() {
  // The instructions for pinning Chrome to the taskbar were only displayed if
  // Chrome wasn't pinned to the taskbar at page construction time.
  bool pin_instructions_shown =
      pinned_state_result_.has_value() && !pinned_state_result_.value();

  std::string histogram_suffix = "Inline";
  histogram_suffix += pin_instructions_shown ? "Combined" : "Default";

  // Closing the page. Record whether the instructions were useful.
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>(
      base::Bind(&RecordDefaultBrowserResult, histogram_suffix))
      ->StartCheckIsDefault();

  if (pin_instructions_shown) {
    // On error, call RecordPinnedResult() with |succeeded| == false.
    base::Closure error_callback =
        base::Bind(&RecordPinnedResult, histogram_suffix, false, false);
    shell_integration::win::GetIsPinnedToTaskbarState(
        GetClonedConnector(), error_callback,
        base::Bind(&RecordPinnedResult, histogram_suffix));
  }
}

void WelcomeWin10Handler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "handleSetDefaultBrowser",
      base::BindRepeating(&WelcomeWin10Handler::HandleSetDefaultBrowser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "handleContinue",
      base::BindRepeating(&WelcomeWin10Handler::HandleContinue,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPinnedToTaskbarState",
      base::BindRepeating(&WelcomeWin10Handler::HandleGetPinnedToTaskbarState,
                          base::Unretained(this)));
}

void WelcomeWin10Handler::OnJavascriptDisallowed() {
  pinned_state_callback_id_.clear();
}

void WelcomeWin10Handler::HandleGetPinnedToTaskbarState(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AllowJavascript();

  // Save the callback id so that the result can be sent back when it is
  // available.
  bool callback_id_found = args->GetString(0, &pinned_state_callback_id_);
  DCHECK(callback_id_found);

  // Send back the result if it is already available.
  if (pinned_state_result_.has_value()) {
    SendPinnedToTaskbarStateResult();
    return;
  }

  // Only wait for a small amount of time for the result. If the timer fires,
  // it will be assumed that Chrome isn't pinned to the taskbar. This is to make
  // sure the instructions are displayed in case it was impossible to determine
  // the pinned state.
  constexpr base::TimeDelta kPinnedToTaskbarTimeout =
      base::TimeDelta::FromMilliseconds(200);
  timer_.Start(FROM_HERE, kPinnedToTaskbarTimeout,
               base::Bind(&WelcomeWin10Handler::OnIsPinnedToTaskbarDetermined,
                          base::Unretained(this), true, false));
}

void WelcomeWin10Handler::HandleSetDefaultBrowser(const base::ListValue* args) {
  base::RecordAction(
      base::UserMetricsAction("Win10WelcomePage_SetAsDefaultBrowser"));
  // The worker owns itself.
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>(
      shell_integration::DefaultWebClientWorkerCallback())
      ->StartSetAsDefault();
}

void WelcomeWin10Handler::HandleContinue(const base::ListValue* args) {
  web_ui()->GetWebContents()->GetController().LoadURL(
      GURL(chrome::kChromeUINewTabURL), content::Referrer(),
      ui::PageTransition::PAGE_TRANSITION_LINK, std::string());
}

void WelcomeWin10Handler::StartIsPinnedToTaskbarCheck() {
  // Assume that Chrome is pinned to the taskbar if an error occurs.
  base::Closure error_callback =
      base::Bind(&WelcomeWin10Handler::OnIsPinnedToTaskbarDetermined,
                 weak_ptr_factory_.GetWeakPtr(), false, true);

  shell_integration::win::GetIsPinnedToTaskbarState(
      GetClonedConnector(), error_callback,
      base::Bind(&WelcomeWin10Handler::OnIsPinnedToTaskbarResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WelcomeWin10Handler::OnIsPinnedToTaskbarResult(bool succeeded,
                                                    bool is_pinned_to_taskbar) {
  // Assume that Chrome is pinned to the taskbar if an error occured.
  OnIsPinnedToTaskbarDetermined(false, !succeeded || is_pinned_to_taskbar);
}

void WelcomeWin10Handler::OnIsPinnedToTaskbarDetermined(
    bool timed_out,
    bool is_pinned_to_taskbar) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Early exit if the pinned_state was already determined.
  if (pinned_state_result_.has_value())
    return;

  UMA_HISTOGRAM_BOOLEAN("Welcome.Win10.PinCheckTimedOut", timed_out);

  // Stop the timer if it's still running.
  timer_.Stop();

  // Cache the value.
  pinned_state_result_.emplace(is_pinned_to_taskbar);

  // If the page already called getPinnedToTaskbarState(), the result can be
  // sent back.
  if (!pinned_state_callback_id_.empty())
    SendPinnedToTaskbarStateResult();
}

void WelcomeWin10Handler::SendPinnedToTaskbarStateResult() {
  ResolveJavascriptCallback(base::Value(pinned_state_callback_id_),
                            base::Value(pinned_state_result_.value()));
}
