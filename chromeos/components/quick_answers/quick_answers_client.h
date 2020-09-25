// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "chromeos/components/quick_answers/result_loader.h"
#include "chromeos/components/quick_answers/understanding/intent_generator.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace chromeos {
namespace quick_answers {

struct QuickAnswer;
struct QuickAnswersRequest;
struct IntentInfo;
enum class IntentType;
enum class ResultType;

// A delegate interface for the QuickAnswersClient.
class QuickAnswersDelegate {
 public:
  QuickAnswersDelegate(const QuickAnswersDelegate&) = delete;
  QuickAnswersDelegate& operator=(const QuickAnswersDelegate&) = delete;

  // Invoked when the |quick_answer| is received. Note that |quick_answer| may
  // be |nullptr| if no answer found for the selected content.
  virtual void OnQuickAnswerReceived(
      std::unique_ptr<QuickAnswer> quick_answer) {}

  // Invoked when the query is rewritten.
  virtual void OnRequestPreprocessFinished(
      const QuickAnswersRequest& processed_request) {}

  // Invoked when feature eligibility changed.
  virtual void OnEligibilityChanged(bool eligible) {}

  // Invoked when there is a network error.
  virtual void OnNetworkError() {}

 protected:
  QuickAnswersDelegate() = default;
  virtual ~QuickAnswersDelegate() = default;
};

// Quick answers client to load and parse quick answer results.
class QuickAnswersClient : public ash::AssistantStateObserver,
                           public ResultLoader::ResultLoaderDelegate {
 public:
  // Method that can be used in tests to change the result loader returned by
  // |CreateResultLoader| in tests.
  using ResultLoaderFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<ResultLoader>()>;

  // Method that can be used in tests to change the intent generator returned by
  // |CreateResultLoader| in tests.
  using IntentGeneratorFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<IntentGenerator>()>;

  QuickAnswersClient(network::mojom::URLLoaderFactory* url_loader_factory,
                     ash::AssistantState* assistant_state,
                     QuickAnswersDelegate* delegate);

  QuickAnswersClient(const QuickAnswersClient&) = delete;
  QuickAnswersClient& operator=(const QuickAnswersClient&) = delete;

  ~QuickAnswersClient() override;

  // AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      chromeos::assistant::AssistantAllowedState state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnAssistantQuickAnswersEnabled(bool enabled) override;
  void OnAssistantStateDestroyed() override;

  // ResultLoaderDelegate:
  void OnNetworkError() override;
  void OnQuickAnswerReceived(
      std::unique_ptr<QuickAnswer> quick_answer) override;

  // Send a quick answer request for preprocessing only.
  void SendRequestForPreprocessing(
      const QuickAnswersRequest& quick_answers_request);
  // Fetch quick answers result from the server.
  void FetchQuickAnswers(const QuickAnswersRequest& processed_request);
  // Send a quick answer request. The request is preprocessed before fetching
  // the result from the server. Virtual for testing.
  virtual void SendRequest(const QuickAnswersRequest& quick_answers_request);

  // User clicks on the Quick Answers result. Virtual for testing.
  virtual void OnQuickAnswerClick(ResultType result_type);

  // Quick Answers is dismissed. Virtual for testing.
  virtual void OnQuickAnswersDismissed(ResultType result_type, bool is_active);

  static void SetResultLoaderFactoryForTesting(
      ResultLoaderFactoryCallback* factory);

  static void SetIntentGeneratorFactoryForTesting(
      IntentGeneratorFactoryCallback* factory);

  static bool IsQuickAnswersAllowedForLocale(const std::string& locale,
                                             const std::string& runtime_locale);

 private:
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest, SendRequest);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest,
                           NotSendRequestForUnknownIntent);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest, PreprocessDefinitionIntent);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest, PreprocessTranslationIntent);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest,
                           PreprocessUnitConversionIntent);

  // Creates a |ResultLoader| instance.
  std::unique_ptr<ResultLoader> CreateResultLoader(IntentType intent_type);

  // Creates an |IntentGenerator| instance.
  std::unique_ptr<IntentGenerator> CreateIntentGenerator(
      const QuickAnswersRequest& request,
      bool skip_fetch);

  void NotifyEligibilityChanged();
  // Preprocesses the |QuickAnswersRequest| and fetch quick answers result. Only
  // preprocesses the request and skip fetching result if |skip_fetch| is true.
  void SendRequestInternal(const QuickAnswersRequest& quick_answers_request,
                           bool skip_fetch);
  void IntentGeneratorCallback(const QuickAnswersRequest& quick_answers_request,
                               bool skip_fetch,
                               const IntentInfo& intent_info);
  base::TimeDelta GetImpressionDuration() const;

  network::mojom::URLLoaderFactory* url_loader_factory_ = nullptr;
  ash::AssistantState* assistant_state_ = nullptr;
  QuickAnswersDelegate* delegate_ = nullptr;
  std::unique_ptr<ResultLoader> result_loader_;
  std::unique_ptr<IntentGenerator> intent_generator_;
  bool assistant_enabled_ = false;
  bool assistant_context_enabled_ = false;
  bool quick_answers_settings_enabled_ = false;
  bool locale_supported_ = false;
  chromeos::assistant::AssistantAllowedState assistant_allowed_state_ =
      chromeos::assistant::AssistantAllowedState::ALLOWED;
  bool is_eligible_ = false;
  // Time when the quick answer is received.
  base::TimeTicks quick_answer_received_time_;

  base::WeakPtrFactory<QuickAnswersClient> weak_factory_{this};
};

}  // namespace quick_answers
}  // namespace chromeos
#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
