// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/ios/browser/ios_translate_driver.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/language_detection/core/browser/language_detection_model_service.h"
#include "components/language_detection/ios/browser/language_detection_model_loader_service_ios.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#import "components/translate/ios/browser/translate_controller.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace translate {

namespace {
// Language name passed to the Translate element for it to detect the language.
const char kAutoDetectionLanguage[] = "auto";

// Based on the cross platform value in
// //components/translate/content/renderer/per_frame_translate_agent.cc
// which checks every 400ms, up to 10 times.
// Double that time to take the script injection into account.
const base::TimeDelta kTimeoutDelay = base::Seconds(8);

}  // namespace

IOSTranslateDriver::IOSTranslateDriver(
    web::WebState* web_state,
    language_detection::LanguageDetectionModelLoaderServiceIOS*
        language_detection_model_service)
    : web_state_(web_state),
      language_detection_model_service_(language_detection_model_service),
      page_seq_no_(0),
      pending_page_seq_no_(-1) {}

void IOSTranslateDriver::Initialize(
    language::UrlLanguageHistogram* url_language_histogram,
    TranslateManager* translate_manager) {
  DCHECK(translate_manager);
  DCHECK(web_state_);
  translate_manager_ = translate_manager->GetWeakPtr();
  web_state_observation_.Observe(web_state_);

  LanguageDetectionModel* language_detection_model = nullptr;
  if (language_detection_model_service_ && IsTFLiteLanguageDetectionEnabled()) {
    language_detection_model =
        language_detection_model_service_->GetLanguageDetectionModel();
  }

  language::IOSLanguageDetectionTabHelper::CreateForWebState(
      web_state_, url_language_histogram, language_detection_model,
      translate_manager_->translate_client()->GetPrefs());
  language_detection_observation_.Observe(
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state_));

  TranslateController::CreateForWebState(web_state_);
  TranslateController::FromWebState(web_state_)->set_observer(this);
}

IOSTranslateDriver::~IOSTranslateDriver() {
}

void IOSTranslateDriver::OnLanguageDetermined(
    const LanguageDetectionDetails& details) {
  if (!translate_manager_)
    return;
  translate_manager_->GetLanguageState()->LanguageDetermined(
      details.adopted_language, true);

  // Don't offer translation on pages with notranslate meta tag.
  if (web_state_ && !details.has_notranslate)
    translate_manager_->InitiateTranslation(details.adopted_language);

  for (auto& observer : language_detection_observers())
    observer.OnLanguageDetermined(details);
}

void IOSTranslateDriver::IOSLanguageDetectionTabHelperWasDestroyed(
    language::IOSLanguageDetectionTabHelper* tab_helper) {
  StopAllObservations();
}

// web::WebStateObserver methods

void IOSTranslateDriver::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (!navigation_context->IsSameDocument()) {
    pending_page_seq_no_ = -1;
    timeout_timer_.Stop();
  }
}

void IOSTranslateDriver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);

  // Interrupt pending translations and reset various data when a navigation
  // happens. Desktop does it by tracking changes in the page ID, and
  // through WebContentObserver, but these concepts do not exist on iOS.
  if (!navigation_context->IsSameDocument()) {
    ++page_seq_no_;
    translate_manager_->set_current_seq_no(page_seq_no_);
  }

  // TODO(crbug.com/41437388): support navigation types, like content/ does.
  const bool reload = ui::PageTransitionCoreTypeIs(
      navigation_context->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  translate_manager_->GetLanguageState()->DidNavigate(
      navigation_context->IsSameDocument(), true, reload, std::string(), false);
}

void IOSTranslateDriver::WebStateDestroyed(web::WebState* web_state) {
  StopAllObservations();
}

// TranslateDriver methods

bool IOSTranslateDriver::IsLinkNavigation() {
  DCHECK(web_state_->IsRealized());
  web::NavigationItem* visible_item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  return visible_item &&
         ui::PageTransitionCoreTypeIs(visible_item->GetTransitionType(),
                                      ui::PAGE_TRANSITION_LINK);
}

void IOSTranslateDriver::OnTranslateEnabledChanged() {
}

void IOSTranslateDriver::OnIsPageTranslatedChanged() {
}

void IOSTranslateDriver::PrepareToTranslatePage(
    int page_seq_no,
    const std::string& original_source_lang,
    const std::string& target_lang,
    bool triggered_from_menu) {
  if (!IsPageValid(page_seq_no))
    return;  // The user navigated away.
  pending_page_seq_no_ = page_seq_no;
  source_language_ = original_source_lang;
  target_language_ = target_lang;
  timeout_timer_.Start(FROM_HERE, kTimeoutDelay,
                       BindOnce(&IOSTranslateDriver::OnTranslationTimeout,
                                weak_ptr_factory_.GetWeakPtr(), page_seq_no));
  // Remove annotations before replacing translated data.
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state_);
  if (manager) {
    manager->RemoveDecorations();
  }
}

void IOSTranslateDriver::TranslatePage(int page_seq_no,
                                       const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  if (!IsPageValid(pending_page_seq_no_))
    return;  // The user navigated away.
  source_language_ = source_lang;
  target_language_ = target_lang;
  TranslateController::FromWebState(web_state_)
      ->InjectTranslateScript(translate_script);
}

void IOSTranslateDriver::OnTranslationTimeout(int page_seq_no) {
  if (!IsPageValid(pending_page_seq_no_) || !IsPageValid(page_seq_no))
    return;  // The user navigated away or timeout is obsolete.

  translate_manager_->PageTranslated(source_language_, target_language_,
                                     TranslateErrors::TRANSLATION_TIMEOUT);
  pending_page_seq_no_ = -1;
  RevertTranslation(page_seq_no);
}

void IOSTranslateDriver::RevertTranslation(int page_seq_no) {
  if (page_seq_no != page_seq_no_)
    return;  // The user navigated away.
  timeout_timer_.Stop();
  TranslateController::FromWebState(web_state_)->RevertTranslation();
}

bool IOSTranslateDriver::IsIncognito() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

const std::string& IOSTranslateDriver::GetContentsMimeType() {
  return web_state_->GetContentsMimeType();
}

const GURL& IOSTranslateDriver::GetLastCommittedURL() const {
  return web_state_->GetLastCommittedURL();
}

const GURL& IOSTranslateDriver::GetVisibleURL() {
  return web_state_->GetVisibleURL();
}

ukm::SourceId IOSTranslateDriver::GetUkmSourceId() {
  return ukm::GetSourceIdForWebStateDocument(web_state_);
}

bool IOSTranslateDriver::HasCurrentPage() const {
  DCHECK(web_state_->IsRealized());
  return (web_state_->GetNavigationManager()->GetVisibleItem() != nullptr);
}

void IOSTranslateDriver::TranslationDidSucceed(
    const std::string& source_lang,
    const std::string& target_lang,
    int page_seq_no,
    const std::string& original_page_language,
    double translation_time) {
  if (!IsPageValid(page_seq_no))
    return;
  std::string actual_source_lang;
  TranslateErrors translate_errors = TranslateErrors::NONE;
  // Translation was successfull; if it was auto, retrieve the source
  // language the Translate Element detected.
  if (source_lang == kAutoDetectionLanguage) {
    actual_source_lang = original_page_language;
    if (actual_source_lang.empty()) {
      translate_errors = TranslateErrors::UNKNOWN_LANGUAGE;
    } else if (actual_source_lang == target_lang) {
      translate_errors = TranslateErrors::IDENTICAL_LANGUAGES;
    }
  } else {
    actual_source_lang = source_lang;
  }
  if (translate_errors == TranslateErrors::NONE)
    ReportTimeToTranslate(translation_time);
  // Notify the manage of completion.
  translate_manager_->PageTranslated(actual_source_lang, target_lang,
                                     translate_errors);
}

bool IOSTranslateDriver::IsPageValid(int page_seq_no) const {
  bool user_navigated_away = page_seq_no != page_seq_no_;
  return !user_navigated_away && web_state_;
}

// TranslateController::Observer implementation.

void IOSTranslateDriver::OnTranslateScriptReady(TranslateErrors error_type,
                                                double load_time,
                                                double ready_time) {
  if (!IsPageValid(pending_page_seq_no_))
    return;

  if (error_type != TranslateErrors::NONE) {
    translate_manager_->PageTranslated(source_language_, target_language_,
                                       error_type);
    return;
  }

  ReportTimeToLoad(load_time);
  ReportTimeToBeReady(ready_time);
  std::string source = (source_language_ != kUnknownLanguageCode)
                           ? source_language_
                           : kAutoDetectionLanguage;
  TranslateController::FromWebState(web_state_)
      ->StartTranslation(source, target_language_);
}

void IOSTranslateDriver::OnTranslateComplete(TranslateErrors error_type,
                                             const std::string& source_language,
                                             double translation_time) {
  if (!IsPageValid(pending_page_seq_no_))
    return;

  if (error_type != TranslateErrors::NONE) {
    translate_manager_->PageTranslated(source_language_, target_language_,
                                       error_type);
    return;
  }

  TranslationDidSucceed(source_language_, target_language_,
                        pending_page_seq_no_, source_language,
                        translation_time);
  pending_page_seq_no_ = -1;
  timeout_timer_.Stop();
}

void IOSTranslateDriver::StopAllObservations() {
  timeout_timer_.Stop();
  language_detection_observation_.Reset();
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

}  // namespace translate
