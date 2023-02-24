// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/per_frame_translate_agent.h"

#include <string>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/translate/content/renderer/isolated_world_util.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_language_detection_details.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using blink::WebDocument;
using blink::WebLanguageDetectionDetails;
using blink::WebLocalFrame;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebVector;

namespace {

// The delay in milliseconds that we'll wait before checking to see if the
// translate library injected in the page is ready.
// TODO(crbug.com/1064974): switch from int time values to base::TimeDelta.
const int kTranslateInitCheckDelayMs = 150;

// The maximum number of times we'll check to see if the translate library
// injected in the page is ready.
const int kMaxTranslateInitCheckAttempts = 5;

// The delay we wait in milliseconds before checking whether the translation has
// finished.
const int kTranslateStatusCheckDelayMs = 400;

// The maximum number of times we'll check whether the translation has
// finished.
const int kMaxTranslateStatusCheckAttempts = 10;

// Language name passed to the Translate element for it to detect the language.
const char kAutoDetectionLanguage[] = "auto";

}  // namespace

namespace translate {

////////////////////////////////////////////////////////////////////////////////
// PerFrameTranslateAgent, public:
PerFrameTranslateAgent::PerFrameTranslateAgent(
    content::RenderFrame* render_frame,
    int world_id,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame), world_id_(world_id) {
  registry->AddInterface<mojom::TranslateAgent>(base::BindRepeating(
      &PerFrameTranslateAgent::BindReceiver, base::Unretained(this)));
}

PerFrameTranslateAgent::~PerFrameTranslateAgent() = default;

// mojom::TranslateAgent implementations.
void PerFrameTranslateAgent::GetWebLanguageDetectionDetails(
    GetWebLanguageDetectionDetailsCallback callback) {
  // Get the document language as set by WebKit from the http-equiv
  // meta tag for "content-language".  This may or may not also
  // have a value derived from the actual Content-Language HTTP
  // header.  The two actually have different meanings (despite the
  // original intent of http-equiv to be an equivalent) with the former
  // being the language of the document and the latter being the
  // language of the intended audience (a distinction really only
  // relevant for things like language textbooks).  This distinction
  // shouldn't affect translation.
  WebLocalFrame* local_frame = render_frame()->GetWebFrame();
  WebDocument document = local_frame->GetDocument();
  WebLanguageDetectionDetails web_detection_details =
      WebLanguageDetectionDetails::CollectLanguageDetectionDetails(document);

  std::move(callback).Run(web_detection_details.content_language.Utf8(),
                          web_detection_details.html_language.Utf8(),
                          web_detection_details.url,
                          web_detection_details.has_no_translate_meta);
}

void PerFrameTranslateAgent::TranslateFrame(const std::string& translate_script,
                                            const std::string& source_lang,
                                            const std::string& target_lang,
                                            TranslateFrameCallback callback) {
  // A similar translation is already under way, nothing to do.
  if (translate_callback_pending_ && target_lang_ == target_lang) {
    // This request is ignored.
    std::move(callback).Run(true /* cancelled */, source_lang, target_lang,
                            TranslateErrors::NONE);
    return;
  }

  // Any pending translation is now irrelevant.
  CancelPendingTranslation();

  // Set our states.
  translate_callback_pending_ = std::move(callback);

  // If the source language is undetermined, we'll let the translate element
  // detect it.
  source_lang_ = (source_lang != kUnknownLanguageCode) ? source_lang
                                                       : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  GURL url(render_frame()->GetWebFrame()->GetDocument().Url());
  ReportPageScheme(url.scheme());

  EnsureIsolatedWorldInitialized(world_id_);

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  TranslateFrameImpl(0 /* try_count */);
}

void PerFrameTranslateAgent::RevertTranslation() {
  if (!IsTranslateLibAvailable()) {
    return;
  }

  CancelPendingTranslation();

  ExecuteScript("cr.googleTranslate.revert()");
}

////////////////////////////////////////////////////////////////////////////////
// PerFrameTranslateAgent, protected:
bool PerFrameTranslateAgent::IsTranslateLibAvailable() {
  return ExecuteScriptAndGetBoolResult(
      "typeof cr != 'undefined' && typeof cr.googleTranslate != 'undefined' && "
      "typeof cr.googleTranslate.translate == 'function'",
      false);
}

bool PerFrameTranslateAgent::IsTranslateLibReady() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.libReady", false);
}

bool PerFrameTranslateAgent::HasTranslationFinished() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.finished", true);
}

bool PerFrameTranslateAgent::HasTranslationFailed() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.error", true);
}

int64_t PerFrameTranslateAgent::GetErrorCode() {
  int64_t error_code =
      ExecuteScriptAndGetIntegerResult("cr.googleTranslate.errorCode");
  DCHECK_LT(error_code, static_cast<int>(TranslateErrors::TRANSLATE_ERROR_MAX));
  return error_code;
}

bool PerFrameTranslateAgent::StartTranslation() {
  return ExecuteScriptAndGetBoolResult(
      BuildTranslationScript(source_lang_, target_lang_), false);
}

std::string PerFrameTranslateAgent::GetPageSourceLanguage() {
  return ExecuteScriptAndGetStringResult("cr.googleTranslate.sourceLang");
}

base::TimeDelta PerFrameTranslateAgent::AdjustDelay(int delay_in_milliseconds) {
  // Just converts |delay_in_milliseconds| without any modification in practical
  // cases. Tests will override this function to return modified value.
  return base::Milliseconds(delay_in_milliseconds);
}

void PerFrameTranslateAgent::ExecuteScript(const std::string& script) {
  EnsureIsolatedWorldInitialized(world_id_);
  WebLocalFrame* local_frame = render_frame()->GetWebFrame();
  if (!local_frame)
    return;

  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  local_frame->ExecuteScriptInIsolatedWorld(
      world_id_, source, blink::BackForwardCacheAware::kAllow);
}

bool PerFrameTranslateAgent::ExecuteScriptAndGetBoolResult(
    const std::string& script,
    bool fallback) {
  EnsureIsolatedWorldInitialized(world_id_);
  WebLocalFrame* local_frame = render_frame()->GetWebFrame();
  if (!local_frame)
    return fallback;

  v8::HandleScope handle_scope(
      local_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      local_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);

  if (result.IsEmpty() || !result->IsBoolean()) {
    return fallback;
  }

  return result.As<v8::Boolean>()->Value();
}

std::string PerFrameTranslateAgent::ExecuteScriptAndGetStringResult(
    const std::string& script) {
  EnsureIsolatedWorldInitialized(world_id_);
  WebLocalFrame* local_frame = render_frame()->GetWebFrame();
  if (!local_frame)
    return std::string();

  v8::Isolate* isolate = local_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      local_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);

  if (result.IsEmpty() || !result->IsString()) {
    return std::string();
  }

  v8::Local<v8::String> v8_str = result.As<v8::String>();
  int length = v8_str->Utf8Length(isolate);
  if (length <= 0)
    return std::string();

  std::string str(static_cast<size_t>(length), '\0');
  v8_str->WriteUtf8(isolate, &str[0], length);
  return str;
}

double PerFrameTranslateAgent::ExecuteScriptAndGetDoubleResult(
    const std::string& script) {
  EnsureIsolatedWorldInitialized(world_id_);
  WebLocalFrame* local_frame = render_frame()->GetWebFrame();
  if (!local_frame)
    return 0.0;

  v8::HandleScope handle_scope(
      local_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      local_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);

  if (result.IsEmpty() || !result->IsNumber()) {
    return 0.0;
  }

  return result.As<v8::Number>()->Value();
}

int64_t PerFrameTranslateAgent::ExecuteScriptAndGetIntegerResult(
    const std::string& script) {
  EnsureIsolatedWorldInitialized(world_id_);
  WebLocalFrame* local_frame = render_frame()->GetWebFrame();
  if (!local_frame)
    return 0;

  v8::HandleScope handle_scope(
      local_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      local_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);

  if (result.IsEmpty() || !result->IsNumber()) {
    return 0;
  }

  return result.As<v8::Integer>()->Value();
}

////////////////////////////////////////////////////////////////////////////////
// PerFrameTranslateAgent, private:
void PerFrameTranslateAgent::CheckTranslateStatus(int check_count) {
  DCHECK_LT(check_count, kMaxTranslateStatusCheckAttempts);
  // First check if there was an error.
  if (HasTranslationFailed()) {
    NotifyBrowserTranslationFailed(
        static_cast<translate::TranslateErrors>(GetErrorCode()));
    return;  // There was an error.
  }

  if (HasTranslationFinished()) {
    std::string actual_source_lang;
    // Translation was successful, if it was auto, retrieve the source
    // language the Translate Element detected.
    if (source_lang_ == kAutoDetectionLanguage) {
      actual_source_lang = GetPageSourceLanguage();
      if (actual_source_lang.empty()) {
        NotifyBrowserTranslationFailed(TranslateErrors::UNKNOWN_LANGUAGE);
        return;
      } else if (actual_source_lang == target_lang_) {
        NotifyBrowserTranslationFailed(TranslateErrors::IDENTICAL_LANGUAGES);
        return;
      }
    } else {
      actual_source_lang = source_lang_;
    }

    if (!translate_callback_pending_) {
      NOTREACHED();
      return;
    }

    // Check JavaScript performance counters for UMA reports.
    ReportTimeToTranslate(
        ExecuteScriptAndGetDoubleResult("cr.googleTranslate.translationTime"));

    // Notify the browser we are done.
    std::move(translate_callback_pending_)
        .Run(false /* cancelled */, actual_source_lang, target_lang_,
             TranslateErrors::NONE);
    return;
  }

  // The translation is still pending, check again later unless we have tried
  // many times already.
  if (++check_count >= kMaxTranslateStatusCheckAttempts) {
    NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_TIMEOUT);
    return;
  }

  // Check again later.
  render_frame()
      ->GetTaskRunner(blink::TaskType::kInternalTranslation)
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PerFrameTranslateAgent::CheckTranslateStatus,
                         weak_method_factory_.GetWeakPtr(), check_count),
          AdjustDelay(kTranslateStatusCheckDelayMs));
}

void PerFrameTranslateAgent::TranslateFrameImpl(int try_count) {
  DCHECK_LT(try_count, kMaxTranslateInitCheckAttempts);
  if (!IsTranslateLibReady()) {
    // There was an error during initialization of library.
    TranslateErrors error =
        static_cast<translate::TranslateErrors>(GetErrorCode());
    if (error != TranslateErrors::NONE) {
      NotifyBrowserTranslationFailed(error);
      return;
    }

    // The library is not ready, try again later, unless we have tried several
    // times unsuccessfully already.
    if (++try_count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_TIMEOUT);
      return;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PerFrameTranslateAgent::TranslateFrameImpl,
                       weak_method_factory_.GetWeakPtr(), try_count),
        AdjustDelay(try_count * kTranslateInitCheckDelayMs));
    return;
  }

  // The library is loaded, and ready for translation now.
  // Check JavaScript performance counters for UMA reports.
  ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  ReportTimeToLoad(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.loadTime"));

  if (!StartTranslation()) {
    DCHECK(HasTranslationFailed());
    NotifyBrowserTranslationFailed(
        static_cast<translate::TranslateErrors>(GetErrorCode()));
    return;
  }
  // Check the status of the translation.
  render_frame()
      ->GetTaskRunner(blink::TaskType::kInternalTranslation)
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PerFrameTranslateAgent::CheckTranslateStatus,
                         weak_method_factory_.GetWeakPtr(), 0),
          AdjustDelay(kTranslateStatusCheckDelayMs));
}

void PerFrameTranslateAgent::NotifyBrowserTranslationFailed(
    TranslateErrors error) {
  DCHECK(translate_callback_pending_);
  // Notify the browser there was an error.
  std::move(translate_callback_pending_)
      .Run(false /* cancelled */, source_lang_, target_lang_, error);
}

void PerFrameTranslateAgent::OnDestruct() {
  delete this;
}

void PerFrameTranslateAgent::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  // Make sure to send the cancelled response back.
  if (translate_callback_pending_) {
    std::move(translate_callback_pending_)
        .Run(true /* cancelled */, source_lang_, target_lang_,
             TranslateErrors::NONE);
  }
  source_lang_.clear();
  target_lang_.clear();
}

void PerFrameTranslateAgent::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::TranslateAgent> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

/* static */
std::string PerFrameTranslateAgent::BuildTranslationScript(
    const std::string& source_lang,
    const std::string& target_lang) {
  return "cr.googleTranslate.translate(" +
         base::GetQuotedJSONString(source_lang) + "," +
         base::GetQuotedJSONString(target_lang) + ")";
}

}  // namespace translate
