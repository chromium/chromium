// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_client.h"

#include <utility>

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/spell_checker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace quick_answers {
namespace {

QuickAnswersClient::ResultLoaderFactoryCallback*
    g_testing_result_factory_callback = nullptr;

QuickAnswersClient::IntentGeneratorFactoryCallback*
    g_testing_intent_generator_factory_callback = nullptr;

}  // namespace

// static
void QuickAnswersClient::SetResultLoaderFactoryForTesting(
    ResultLoaderFactoryCallback* factory) {
  g_testing_result_factory_callback = factory;
}

void QuickAnswersClient::SetIntentGeneratorFactoryForTesting(
    IntentGeneratorFactoryCallback* factory) {
  g_testing_intent_generator_factory_callback = factory;
}

QuickAnswersClient::QuickAnswersClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    QuickAnswersDelegate* delegate)
    : url_loader_factory_(url_loader_factory),
      delegate_(delegate),
      spell_checker_(std::make_unique<SpellChecker>(url_loader_factory)) {}

QuickAnswersClient::~QuickAnswersClient() = default;

void QuickAnswersClient::SendRequestForPreprocessing(
    const QuickAnswersRequest& quick_answers_request) {
  SendRequestInternal(quick_answers_request, /*skip_fetch=*/true);
}

void QuickAnswersClient::FetchQuickAnswers(
    const QuickAnswersRequest& preprocessed_request) {
  DCHECK(!preprocessed_request.preprocessed_output.query.empty());

  result_loader_ = CreateResultLoader(
      preprocessed_request.preprocessed_output.intent_info.intent_type);
  // Load and parse search result.
  result_loader_->Fetch(preprocessed_request.preprocessed_output);
}

void QuickAnswersClient::SendRequest(
    const QuickAnswersRequest& quick_answers_request) {
  SendRequestInternal(quick_answers_request, /*skip_fetch=*/false);
}

void QuickAnswersClient::OnQuickAnswerClick(ResultType result_type) {
  RecordClick(result_type, GetImpressionDuration());
}

void QuickAnswersClient::OnQuickAnswersDismissed(ResultType result_type,
                                                 bool is_active) {
  if (is_active)
    RecordActiveImpression(result_type, GetImpressionDuration());
}

std::unique_ptr<ResultLoader> QuickAnswersClient::CreateResultLoader(
    IntentType intent_type) {
  if (g_testing_result_factory_callback)
    return g_testing_result_factory_callback->Run();
  return ResultLoader::Create(intent_type, url_loader_factory_, this);
}

std::unique_ptr<IntentGenerator> QuickAnswersClient::CreateIntentGenerator(
    const QuickAnswersRequest& request,
    bool skip_fetch) {
  if (g_testing_intent_generator_factory_callback)
    return g_testing_intent_generator_factory_callback->Run();
  return std::make_unique<IntentGenerator>(
      spell_checker_ ? spell_checker_->GetWeakPtr() : nullptr,
      base::BindOnce(&QuickAnswersClient::IntentGeneratorCallback,
                     weak_factory_.GetWeakPtr(), request, skip_fetch));
}

void QuickAnswersClient::OnNetworkError() {
  DCHECK(delegate_);
  delegate_->OnNetworkError();
}

void QuickAnswersClient::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswersSession> quick_answers_session) {
  DCHECK(delegate_);
  quick_answer_received_time_ = base::TimeTicks::Now();
  delegate_->OnQuickAnswerReceived(std::move(quick_answers_session));
}

void QuickAnswersClient::SendRequestInternal(
    const QuickAnswersRequest& quick_answers_request,
    bool skip_fetch) {
  RecordSelectedTextLength(quick_answers_request.selected_text.length());

  // Generate intent from |quick_answers_request|.
  intent_generator_ = CreateIntentGenerator(quick_answers_request, skip_fetch);
  intent_generator_->GenerateIntent(quick_answers_request);
}

void QuickAnswersClient::IntentGeneratorCallback(
    const QuickAnswersRequest& quick_answers_request,
    bool skip_fetch,
    const IntentInfo& intent_info) {
  DCHECK(delegate_);

  // Preprocess the request.
  QuickAnswersRequest processed_request = quick_answers_request;
  processed_request.preprocessed_output = PreprocessRequest(intent_info);

  delegate_->OnRequestPreprocessFinished(processed_request);

  if (QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    RecordIntentType(intent_info.intent_type);
    if (intent_info.intent_type == IntentType::kUnknown) {
      // Don't fetch answer if no intent is generated.
      return;
    }
  }

  RecordRequestTextLength(intent_info.intent_type,
                          quick_answers_request.selected_text.length());

  if (!skip_fetch)
    FetchQuickAnswers(processed_request);
}

base::TimeDelta QuickAnswersClient::GetImpressionDuration() const {
  // Use default 0 duration.
  base::TimeDelta duration;
  if (!quick_answer_received_time_.is_null()) {
    // Fetch finish, set the duration to be between fetch finish and now.
    duration = base::TimeTicks::Now() - quick_answer_received_time_;
  }
  return duration;
}

}  // namespace quick_answers
