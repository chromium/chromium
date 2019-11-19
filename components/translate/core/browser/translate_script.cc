// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_script.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace translate {

namespace {

const int kExpirationDelayDays = 1;

}  // namespace

const char TranslateScript::kScriptURL[] =
    "https://translate.googleapis.com/translate_a/element.js";
const char TranslateScript::kRequestHeader[] =
    "Google-Translate-Element-Mode: library";
const char TranslateScript::kAlwaysUseSslQueryName[] = "aus";
const char TranslateScript::kAlwaysUseSslQueryValue[] = "true";
const char TranslateScript::kCallbackQueryName[] = "cb";
const char TranslateScript::kCallbackQueryValue[] =
    "cr.googleTranslate.onTranslateElementLoad";
const char TranslateScript::kCssLoaderCallbackQueryName[] = "clc";
const char TranslateScript::kCssLoaderCallbackQueryValue[] =
    "cr.googleTranslate.onLoadCSS";
const char TranslateScript::kJavascriptLoaderCallbackQueryName[] = "jlc";
const char TranslateScript::kJavascriptLoaderCallbackQueryValue[] =
    "cr.googleTranslate.onLoadJavascript";
const char kTranslateServerStudy[] = "TranslateServerStudy";
const char kServerParams[] = "server_params";

TranslateScript::TranslateScript()
    : expiration_delay_(base::TimeDelta::FromDays(kExpirationDelayDays)) {}

TranslateScript::~TranslateScript() {
}

void TranslateScript::Request(const RequestCallback& callback,
                              bool is_incognito) {
  script_fetch_start_time_ = base::Time::Now().ToJsTime();

  DCHECK(data_.empty()) << "Do not fetch the script if it is already fetched";
  callback_list_.push_back(callback);

  if (fetcher_) {
    // If there is already a request in progress, do nothing. |callback| will be
    // run on completion.
    return;
  }

  GURL translate_script_url = GetTranslateScriptURL();

  fetcher_ = std::make_unique<TranslateURLFetcher>();
  fetcher_->set_extra_request_header(kRequestHeader);
  fetcher_->Request(translate_script_url,
                    base::BindOnce(&TranslateScript::OnScriptFetchComplete,
                                   base::Unretained(this)),
                    is_incognito);
}

// static
GURL TranslateScript::GetTranslateScriptURL() {
  GURL translate_script_url;
  // Check if command-line contains an alternative URL for translate service.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(translate::switches::kTranslateScriptURL)) {
    translate_script_url = GURL(command_line.GetSwitchValueASCII(
        translate::switches::kTranslateScriptURL));
    if (!translate_script_url.is_valid() ||
        !translate_script_url.query().empty()) {
      LOG(WARNING) << "The following translate URL specified at the "
                   << "command-line is invalid: "
                   << translate_script_url.spec();
      translate_script_url = GURL();
    }
  }

  // Use default URL when command-line argument is not specified, or specified
  // URL is invalid.
  if (translate_script_url.is_empty())
    translate_script_url = GURL(kScriptURL);

  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kCallbackQueryName,
      kCallbackQueryValue);
  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kAlwaysUseSslQueryName,
      kAlwaysUseSslQueryValue);
  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kCssLoaderCallbackQueryName,
      kCssLoaderCallbackQueryValue);
  translate_script_url = net::AppendQueryParameter(
      translate_script_url,
      kJavascriptLoaderCallbackQueryName,
      kJavascriptLoaderCallbackQueryValue);

  translate_script_url = AddHostLocaleToUrl(translate_script_url);
  translate_script_url = AddApiKeyToUrl(translate_script_url);

  return translate_script_url;
}

void TranslateScript::OnScriptFetchComplete(bool success,
                                            const std::string& data) {
  std::unique_ptr<const TranslateURLFetcher> delete_ptr(fetcher_.release());

  if (success) {
    DCHECK(data_.empty());
    // Insert variable definitions on API Key and security origin.
    data_ = base::StringPrintf("var translateApiKey = '%s';\n",
                               google_apis::GetAPIKey().c_str());

    // Insert server params to pass experimental params to google translate
    // server.
    std::string server_params;
    std::map<std::string, std::string> params;
    if (variations::GetVariationParams(kTranslateServerStudy, &params)) {
      server_params = params[kServerParams];
    }
    base::StringAppendF(
        &data_, "var gtTimeInfo = {'fetchStart': %0.f, 'fetchEnd': %0.f};\n",
        script_fetch_start_time_, base::Time::Now().ToJsTime());
    base::StringAppendF(&data_, "var serverParams = '%s';\n",
                        server_params.c_str());

    GURL security_origin = translate::GetTranslateSecurityOrigin();
    base::StringAppendF(
        &data_, "var securityOrigin = '%s';", security_origin.spec().c_str());

    // Load embedded translate.js.
    data_.append(ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_TRANSLATE_JS));

#if defined(OS_IOS)
    // Append snippet to install callbacks on translate.js if available.
    const char* install_callbacks =
        "try {"
        "  __gCrWeb.translate.installCallbacks();"
        "} catch (error) {};";
    base::StringPiece(install_callbacks).AppendToString(&data_);
#endif  // defined(OS_IOS)

    // Wrap |data| in try/catch block to handle unexpected script errors.
    const char* format =
        "try {"
        "  %s;"
        "} catch (error) {"
        "  cr.googleTranslate.onTranslateElementError(error);"
        "};";
    base::StringAppendF(&data_, format, data.c_str());

    // We'll expire the cached script after some time, to make sure long
    // running browsers still get fixes that might get pushed with newer
    // scripts.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TranslateScript::Clear,
                       weak_method_factory_.GetWeakPtr()),
        expiration_delay_);
  }

  for (auto it = callback_list_.begin(); it != callback_list_.end(); ++it) {
    it->Run(success, data);
  }
  callback_list_.clear();
}

}  // namespace translate
