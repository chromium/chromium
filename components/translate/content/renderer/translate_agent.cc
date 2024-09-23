// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_agent.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/content/renderer/language_detection_agent.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/content/renderer/isolated_world_util.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_language_detection_details.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/gurl.h"
#include "url/url_constants.h"
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
const int kTranslateInitCheckDelayMs = 150;

// The maximum number of times we'll check to see if the translate library
// injected in the page is ready.
const int kMaxTranslateInitCheckAttempts = 5;

// The delay we wait in milliseconds before checking whether the translation has
// finished.
const int kTranslateStatusCheckDelayMs = 400;

// Language name passed to the Translate element for it to detect the language.
const char kAutoDetectionLanguage[] = "auto";

// The current CLD model version.
constexpr char kCLDModelVersion[] = "CLD3";

// Returns the language detection model that is shared across the RenderFrames
// in the renderer.
translate::LanguageDetectionModel& GetLanguageDetectionModel() {
  static base::NoDestructor<translate::LanguageDetectionModel> instance(
      language_detection::GetLanguageDetectionModel());
  return *instance;
}

// Returns if the language detection should be overridden so that a default
// result is returned immediately.
bool ShouldOverrideLanguageDetectionForTesting() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kOverrideLanguageDetection)) {
    return true;
  }
  return false;
}

}  // namespace

namespace translate {

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, public:
TranslateAgent::TranslateAgent(content::RenderFrame* render_frame, int world_id)
    : content::RenderFrameObserver(render_frame),
      world_id_(world_id),
      language_detection_agent_(
          IsTFLiteLanguageDetectionEnabled()
              ? new language_detection::LanguageDetectionAgent(render_frame)
              : nullptr) {
  translate_task_runner_ = this->render_frame()->GetTaskRunner(
      blink::TaskType::kInternalTranslation);
}

TranslateAgent::~TranslateAgent() {}

void TranslateAgent::SeedLanguageDetectionModelForTesting(
    base::File model_file) {
  language_detection_agent_->UpdateLanguageDetectionModel(
      std::move(model_file));
}

void TranslateAgent::PrepareForUrl(const GURL& url) {
  // Navigated to a new url, reset current page translation.
  ResetPage();
}

void TranslateAgent::PageCaptured(
    scoped_refptr<const base::RefCountedString16> contents) {
  TRACE_EVENT("browser", "TranslateAgent::PageCaptured");
  // Get the document language as set by WebKit from the http-equiv
  // meta tag for "content-language".  This may or may not also
  // have a value derived from the actual Content-Language HTTP
  // header.  The two actually have different meanings (despite the
  // original intent of http-equiv to be an equivalent) with the former
  // being the language of the document and the latter being the
  // language of the intended audience (a distinction really only
  // relevant for things like language textbooks). This distinction
  // shouldn't affect translation.
  if (!contents) {
    return;
  }
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebDocument document = main_frame->GetDocument();
  GURL url = GURL(document.Url());
  // Limit detection to URLs that only detect the language of the content if the
  // page is potentially a candidate for translation.  This should be strictly a
  // subset of the conditions in TranslateService::IsTranslatableURL, however,
  // due to layering they cannot be identical. Critically, this list should
  // never filter anything that is eligible for translation. Under filtering is
  // ok as the translate service will make the final call and only results in a
  // slight overhead in running the model when unnecessary.
  if (url.is_empty() || url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) || url.IsAboutBlank()) {
    return;
  }

  page_contents_length_ = contents->as_string().size();

  WebLanguageDetectionDetails web_detection_details =
      WebLanguageDetectionDetails::CollectLanguageDetectionDetails(document);
  WebLanguageDetectionDetails::RecordAcceptLanguageAndXmlHtmlLangMetric(
      document);

  std::string content_language = web_detection_details.content_language.Utf8();
  std::string html_lang = web_detection_details.html_language.Utf8();
  std::string model_detected_language;
  bool is_model_reliable = false;
  std::string detection_model_version;
  float model_reliability_score = 0.0;

  if (ShouldOverrideLanguageDetectionForTesting()) {
    std::string language = "fr";
    LanguageDetectionDetails details;
    details.adopted_language = language;
    details.contents = contents->as_string();
    details.has_run_lang_detection = true;
    ResetPage();

    last_details_ = std::move(details);
    RenewPageRegistration();
    return;
  }

  LanguageDetectionDetails details;
  std::string language;
  if (page_contents_length_ == 0) {
    // If captured content is empty do not run language detection and
    // only use page-provided languages.
    language = translate::DeterminePageLanguageNoModel(
        content_language, html_lang,
        translate::LanguageVerificationType::kNoPageContent);
  } else if (translate::IsTFLiteLanguageDetectionEnabled()) {
    // Use TFLite and page contents to assist with language detection.
    translate::LanguageDetectionModel& language_detection_model =
        GetLanguageDetectionModel();
    bool is_available = language_detection_model.IsAvailable();
    language =
        is_available
            ? language_detection_model.DeterminePageLanguage(
                  content_language, html_lang, contents->as_string(),
                  &model_detected_language, &is_model_reliable,
                  model_reliability_score)
            // If the model is not available do not run language
            // detection and only use page-provided languages.
            : translate::DeterminePageLanguageNoModel(
                  content_language, html_lang,
                  translate::LanguageVerificationType::kModelNotAvailable);
    UMA_HISTOGRAM_BOOLEAN(
        "LanguageDetection.TFLiteModel.WasModelAvailableForDetection",
        is_available);
    UMA_HISTOGRAM_BOOLEAN(
        "LanguageDetection.TFLiteModel.WasModelUnavailableDueToDeferredLoad",
        !is_available &&
            language_detection_agent_->waiting_for_first_foreground());
    detection_model_version = language_detection_model.GetModelVersion();
    details.has_run_lang_detection = true;
  } else {
    // Use CLD3 and page contents to assist with language detection.
    language = DeterminePageLanguage(
        content_language, html_lang, contents->as_string(),
        &model_detected_language, &is_model_reliable, model_reliability_score);
    detection_model_version = kCLDModelVersion;
    details.has_run_lang_detection = true;
  }

  if (language.empty())
    return;

  details.time = base::Time::Now();
  details.url = web_detection_details.url;
  details.content_language = content_language;
  details.model_detected_language = model_detected_language;
  details.is_model_reliable = is_model_reliable;
  details.has_notranslate = web_detection_details.has_no_translate_meta;
  details.html_root_language = html_lang;
  details.adopted_language = language;
  details.model_reliability_score = model_reliability_score;
  details.detection_model_version = detection_model_version;

  // TODO(hajimehoshi): If this affects performance, it should be set only if
  // translate-internals tab exists.
  details.contents = contents->as_string();

  // For the same render frame with the same url, each time when its texts are
  // captured, it should be treated as a new page to do translation.
  ResetPage();

  last_details_ = std::move(details);
  RenewPageRegistration();
}

void TranslateAgent::RenewPageRegistration() {
  if (!last_details_.has_value()) {
    return;
  }

  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    return;
  }

  LanguageDetectionDetails details = std::move(*last_details_);

  ResetPage();
  GetTranslateHandler()->RegisterPage(
      receiver_.BindNewPipeAndPassRemote(
          main_frame->GetTaskRunner(blink::TaskType::kInternalTranslation)),
      details, !details.has_notranslate && !details.adopted_language.empty());

  last_details_ = std::move(details);
}

void TranslateAgent::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  // Make sure to send the cancelled response back.
  if (translate_callback_pending_) {
    std::move(translate_callback_pending_)
        .Run(true, source_lang_, target_lang_, TranslateErrors::NONE);
  }
  source_lang_.clear();
  target_lang_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, protected:
bool TranslateAgent::IsTranslateLibAvailable() {
  return ExecuteScriptAndGetBoolResult(
      "typeof cr != 'undefined' && typeof cr.googleTranslate != 'undefined' && "
      "typeof cr.googleTranslate.translate == 'function'",
      false);
}

bool TranslateAgent::IsTranslateLibReady() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.libReady", false);
}

bool TranslateAgent::HasTranslationFinished() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.finished", true);
}

bool TranslateAgent::HasTranslationFailed() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.error", true);
}

int64_t TranslateAgent::GetErrorCode() {
  int64_t error_code =
      ExecuteScriptAndGetIntegerResult("cr.googleTranslate.errorCode");
  DCHECK_LT(error_code, static_cast<int>(TranslateErrors::TRANSLATE_ERROR_MAX));
  return error_code;
}

bool TranslateAgent::StartTranslation() {
  return ExecuteScriptAndGetBoolResult(
      BuildTranslationScript(source_lang_, target_lang_), false);
}

std::string TranslateAgent::GetPageSourceLanguage() {
  return ExecuteScriptAndGetStringResult("cr.googleTranslate.sourceLang");
}

base::TimeDelta TranslateAgent::AdjustDelay(int delay_in_milliseconds) {
  // Just converts |delay_in_milliseconds| without any modification in practical
  // cases. Tests will override this function to return modified value.
  return base::Milliseconds(delay_in_milliseconds);
}

void TranslateAgent::ExecuteScript(const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  main_frame->ExecuteScriptInIsolatedWorld(
      world_id_, source, blink::BackForwardCacheAware::kAllow);
}

bool TranslateAgent::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                   bool fallback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return fallback;

  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsBoolean()) {
    return fallback;
  }

  return result.As<v8::Boolean>()->Value();
}

std::string TranslateAgent::ExecuteScriptAndGetStringResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return std::string();

  v8::Isolate* isolate = main_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
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

double TranslateAgent::ExecuteScriptAndGetDoubleResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return 0.0;

  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsNumber()) {
    return 0.0;
  }

  return result.As<v8::Number>()->Value();
}

int64_t TranslateAgent::ExecuteScriptAndGetIntegerResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return 0;

  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsNumber()) {
    return 0;
  }

  return result.As<v8::Integer>()->Value();
}

// mojom::TranslateAgent implementations.
void TranslateAgent::TranslateFrame(const std::string& translate_script,
                                    const std::string& source_lang,
                                    const std::string& target_lang,
                                    TranslateFrameCallback callback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    // Cancelled.
    std::move(callback).Run(true, source_lang, target_lang,
                            TranslateErrors::NONE);
    return;  // We navigated away, nothing to do.
  }

  // A similar translation is already under way, nothing to do.
  if (translate_callback_pending_ && target_lang_ == target_lang) {
    // This request is ignored.
    std::move(callback).Run(true, source_lang, target_lang,
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

  // Set up v8 isolated world.
  EnsureIsolatedWorldInitialized(world_id_);

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  TranslatePageImpl(0);
}

void TranslateAgent::RevertTranslation() {
  if (!IsTranslateLibAvailable()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  CancelPendingTranslation();

  ExecuteScript("cr.googleTranslate.revert()");
}

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, private:
void TranslateAgent::CheckTranslateStatus() {
  // First check if there was an error.
  if (HasTranslationFailed()) {
    NotifyBrowserTranslationFailed(
        static_cast<translate::TranslateErrors>(GetErrorCode()));
    return;  // There was an error.
  }

  if (HasTranslationFinished()) {
    std::string actual_source_lang;
    // Translation was successfull, if it was auto, retrieve the source
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
      NOTREACHED_IN_MIGRATION();
      return;
    }

    // Check JavaScript performance counters for UMA reports.
    ReportTimeToTranslate(
        ExecuteScriptAndGetDoubleResult("cr.googleTranslate.translationTime"));
    ReportTranslatedLanguageDetectionContentLength(page_contents_length_);

    // Notify the browser we are done.
    std::move(translate_callback_pending_)
        .Run(false, actual_source_lang, target_lang_, TranslateErrors::NONE);
    return;
  }

  // The translation is still pending, check again later.
  translate_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TranslateAgent::CheckTranslateStatus,
                     weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateAgent::TranslatePageImpl(int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
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
    if (++count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_TIMEOUT);
      return;
    }
    translate_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TranslateAgent::TranslatePageImpl,
                       weak_method_factory_.GetWeakPtr(), count),
        AdjustDelay(count * kTranslateInitCheckDelayMs));
    return;
  }

  // The library is loaded, and ready for translation now.
  // Check JavaScript performance counters for UMA reports.
  ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  ReportTimeToLoad(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.loadTime"));

  if (!StartTranslation()) {
    CheckTranslateStatus();
    return;
  }
  // Check the status of the translation.
  translate_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TranslateAgent::CheckTranslateStatus,
                     weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateAgent::NotifyBrowserTranslationFailed(TranslateErrors error) {
  DCHECK(translate_callback_pending_);
  // Notify the browser there was an error.
  std::move(translate_callback_pending_)
      .Run(false, source_lang_, target_lang_, error);
}

const mojo::Remote<mojom::ContentTranslateDriver>&
TranslateAgent::GetTranslateHandler() {
  if (translate_handler_) {
    if (translate_handler_.is_connected()) {
      return translate_handler_;
    }
    // The translate handler can become unbound or disconnected in testing
    // so this catches that case and reconnects so `this` can connect to
    // the driver in the browser.
    translate_handler_.reset();
  }

  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      translate_handler_.BindNewPipeAndPassReceiver());
  return translate_handler_;
}

void TranslateAgent::ResetPage() {
  last_details_ = {};
  receiver_.reset();
  translate_callback_pending_.Reset();
  CancelPendingTranslation();
}

void TranslateAgent::OnDestruct() {
  delete this;
}

/* static */
std::string TranslateAgent::BuildTranslationScript(
    const std::string& source_lang,
    const std::string& target_lang) {
  return "cr.googleTranslate.translate(" +
         base::GetQuotedJSONString(source_lang) + "," +
         base::GetQuotedJSONString(target_lang) + ")";
}

}  // namespace translate
