// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/js_translate_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

namespace {
// Prefix for the translate javascript commands. Must be kept in sync with
// translate_ios.js.
const char kCommandPrefix[] = "translate";
}

TranslateController::TranslateController(web::WebState* web_state,
                                         JsTranslateManager* manager)
    : web_state_(web_state),
      observer_(nullptr),
      js_manager_(manager),
      weak_method_factory_(this) {
  DCHECK(js_manager_);
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  subscription_ = web_state_->AddScriptCommandCallback(
      base::Bind(
          [](TranslateController* ptr, const base::DictionaryValue& command,
             const GURL& page_url, bool user_is_interacting,
             web::WebFrame* sender_frame) {
            ptr->OnJavascriptCommandReceived(command, page_url,
                                             user_is_interacting, sender_frame);
          },
          base::Unretained(this)),
      kCommandPrefix);
}

TranslateController::~TranslateController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void TranslateController::InjectTranslateScript(
    const std::string& translate_script) {
  [js_manager_ setScript:base::SysUTF8ToNSString(translate_script)];
  [js_manager_ inject];
}

void TranslateController::RevertTranslation() {
  [js_manager_ revertTranslation];
}

void TranslateController::StartTranslation(const std::string& source_language,
                                           const std::string& target_language) {
  [js_manager_ startTranslationFrom:source_language to:target_language];
}

void TranslateController::SetJsTranslateManagerForTesting(
    JsTranslateManager* manager) {
  js_manager_.reset(manager);
}

bool TranslateController::OnJavascriptCommandReceived(
    const base::DictionaryValue& command,
    const GURL& page_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  if (!sender_frame->IsMainFrame()) {
    // Translate is only supported on main frame.
    return false;
  }
  const base::Value* value = nullptr;
  command.Get("command", &value);
  if (!value) {
    return false;
  }

  std::string out_string;
  value->GetAsString(&out_string);
  if (out_string == "translate.ready")
    return OnTranslateReady(command);
  if (out_string == "translate.status")
    return OnTranslateComplete(command);
  if (out_string == "translate.loadjavascript")
    return OnTranslateLoadJavaScript(command);
  if (out_string == "translate.sendrequest")
    return OnTranslateSendRequest(command);

  return false;
}

bool TranslateController::OnTranslateReady(
    const base::DictionaryValue& command) {
  double error_code = 0.;
  double load_time = 0.;
  double ready_time = 0.;

  if (!command.HasKey("errorCode") ||
      !command.GetDouble("errorCode", &error_code) ||
      error_code < TranslateErrors::NONE ||
      error_code >= TranslateErrors::TRANSLATE_ERROR_MAX) {
    return false;
  }

  TranslateErrors::Type error_type =
      static_cast<TranslateErrors::Type>(error_code);
  if (error_type == TranslateErrors::NONE) {
    if (!command.HasKey("loadTime") || !command.HasKey("readyTime")) {
      return false;
    }
    command.GetDouble("loadTime", &load_time);
    command.GetDouble("readyTime", &ready_time);
  }
  if (observer_)
    observer_->OnTranslateScriptReady(error_type, load_time, ready_time);
  return true;
}

bool TranslateController::OnTranslateComplete(
    const base::DictionaryValue& command) {
  double error_code = 0.;
  std::string original_language;
  double translation_time = 0.;

  if (!command.HasKey("errorCode") ||
      !command.GetDouble("errorCode", &error_code) ||
      error_code < TranslateErrors::NONE ||
      error_code >= TranslateErrors::TRANSLATE_ERROR_MAX) {
    return false;
  }

  TranslateErrors::Type error_type =
      static_cast<TranslateErrors::Type>(error_code);
  if (error_type == TranslateErrors::NONE) {
    if (!command.HasKey("originalPageLanguage") ||
        !command.HasKey("translationTime")) {
      return false;
    }
    command.GetString("originalPageLanguage", &original_language);
    command.GetDouble("translationTime", &translation_time);
  }

  if (observer_)
    observer_->OnTranslateComplete(error_type, original_language,
                                   translation_time);
  return true;
}

bool TranslateController::OnTranslateLoadJavaScript(
    const base::DictionaryValue& command) {
  std::string url;
  if (!command.HasKey("url") || !command.GetString("url", &url)) {
    return false;
  }

  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url.find(security_origin.spec()) || script_fetcher_) {
    return false;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);

  script_fetcher_ = network::SimpleURLLoader::Create(
      std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);
  script_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      web_state_->GetBrowserState()->GetURLLoaderFactory(),
      base::BindOnce(&TranslateController::OnScriptFetchComplete,
                     base::Unretained(this)));

  return true;
}

bool TranslateController::OnTranslateSendRequest(
    const base::DictionaryValue& command) {
  std::string method;
  if (!command.HasKey("method") || !command.GetString("method", &method)) {
    return false;
  }
  std::string url;
  if (!command.HasKey("url") || !command.GetString("url", &url)) {
    return false;
  }
  std::string body;
  if (!command.HasKey("body") || !command.GetString("body", &body)) {
    return false;
  }
  double request_id;
  if (!command.HasKey("requestID") ||
      !command.GetDouble("requestID", &request_id)) {
    return false;
  }

  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url.find(security_origin.spec())) {
    return false;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = method;
  request->url = GURL(url);
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  auto fetcher = network::SimpleURLLoader::Create(std::move(request),
                                                  NO_TRAFFIC_ANNOTATION_YET);
  fetcher->AttachStringForUpload(body, "application/x-www-form-urlencoded");
  auto* raw_fetcher = fetcher.get();
  auto pair = request_fetchers_.insert(std::move(fetcher));
  raw_fetcher->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      web_state_->GetBrowserState()->GetURLLoaderFactory(),
      base::BindOnce(&TranslateController::OnRequestFetchComplete,
                     base::Unretained(this), pair.first, url,
                     static_cast<int>(request_id)));
  return true;
}

void TranslateController::OnScriptFetchComplete(
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    web_state_->ExecuteJavaScript(base::UTF8ToUTF16(*response_body));
  }
  script_fetcher_.reset();
}

void TranslateController::OnRequestFetchComplete(
    std::set<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    std::string url,
    int request_id,
    std::unique_ptr<std::string> response_body) {
  const std::unique_ptr<network::SimpleURLLoader>& url_loader = *it;

  // |ResponseInfo()| may be a nullptr if response is incomplete.
  int response_code = 0;
  std::string status_text;
  const network::mojom::URLResponseHead* response_head =
      url_loader->ResponseInfo();
  if (response_head && response_head->headers) {
    net::HttpResponseHeaders* headers = response_head->headers.get();
    response_code = headers->response_code();
    status_text = headers->GetStatusText();
  }

  // Escape the returned string so it can be parsed by JSON.parse.
  std::string response_text = response_body ? *response_body : "";
  std::string escaped_response_text;
  base::EscapeJSONString(response_text, /*put_in_quotes=*/false,
                         &escaped_response_text);

  // Return the response details to function defined in translate_ios.js.
  std::string script = base::StringPrintf(
      "__gCrWeb.translate.handleResponse('%s', %d, %d, '%s', '%s', '%s')",
      url.c_str(), request_id, response_code, status_text.c_str(),
      url_loader->GetFinalURL().spec().c_str(), escaped_response_text.c_str());
  web_state_->ExecuteJavaScript(base::UTF8ToUTF16(script));

  request_fetchers_.erase(it);
}

// web::WebStateObserver implementation.

void TranslateController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;

  request_fetchers_.clear();
  script_fetcher_.reset();
}

void TranslateController::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument()) {
    request_fetchers_.clear();
    script_fetcher_.reset();
  }
}

}  // namespace translate
