// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_NEXT_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_NEXT_WEB_DIALOG_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/webui/hats/hats_page_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Browser;
class Profile;

namespace views {
class Widget;
}  // namespace views

// A dialog for displaying a Happiness Tracking Survey (HaTS) NEXT survey to
// the user. The dialog presents a WebContents which connects to a publicly
// accessible, Chrome specific, webpage which is responsible for displaying the
// survey to users. The webpage has additional logic to provide information to
// this dialog via URL fragments, such as whether a survey is ready to be shown
// to the user.
class HatsNextWebDialog : public views::BubbleDialogDelegateView,
                          public content::WebContentsDelegate,
                          public ProfileObserver,
                          public HatsPageHandlerDelegate {
  METADATA_HEADER(HatsNextWebDialog, views::BubbleDialogDelegateView)

 public:
  HatsNextWebDialog(Browser* browser,
                    const std::string& trigger_id,
                    const std::optional<std::string>& hats_histogram_name,
                    const std::optional<uint64_t> hats_survey_ukm_id,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data);
  ~HatsNextWebDialog() override;
  HatsNextWebDialog(const HatsNextWebDialog&) = delete;
  HatsNextWebDialog& operator=(const HatsNextWebDialog&) = delete;

  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // HatsPageHandlerDelegate:
  std::string GetTriggerId() override;
  bool GetEnableTesting() override;
  std::vector<std::string> GetLanguageList() override;
  base::Value::Dict GetProductSpecificDataJson() override;
  std::optional<std::string> GetHistogramName();
  void OnSurveyLoaded() override;
  void OnSurveyCompleted() override;
  void OnSurveyClosed() override;
  void OnSurveyQuestionAnswered(const std::string& state);

  static bool ParseSurveyQuestionAnswer(const std::string& input,
                                        int* question,
                                        std::vector<int>* answers);

  static uint64_t EncodeUkmQuestionAnswers(
      const std::vector<int>& question_answers);

  enum class SurveyHistogramEnumeration {
    kSurveyLoadedEnumeration = 2,
    kSurveyCompletedEnumeration = 3,
    kSurveyQuestionAnswerParseError = 8,
    kSurveyUnknownState = 9
  };

 protected:
  friend class MockHatsNextWebDialog;
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest, SurveyLoaded);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest,
                           SurveyLoadedWithHistogramName);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest,
                           SurveyQuestionAnsweredFirstQuestionHistograms);
  FRIEND_TEST_ALL_PREFIXES(
      HatsNextWebDialogBrowserTest,
      SurveyQuestionAnsweredSingleSelectQuestionHistograms);
  FRIEND_TEST_ALL_PREFIXES(
      HatsNextWebDialogBrowserTest,
      SurveyQuestionAnsweredMultipleSelectQuestionHistograms);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest,
                           SurveyQuestionAnsweredMultipleQuestionsHistograms);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest,
                           SurveyLoadedWithHistogramName);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest,
                           SurveyQuestionAnsweredMultipleQuestions);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest, DialogResize);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest, MaximumSize);
  FRIEND_TEST_ALL_PREFIXES(HatsNextWebDialogBrowserTest, ZoomLevel);

  HatsNextWebDialog(Browser* browser,
                    const std::string& trigger_id,
                    const std::optional<std::string>& hats_histogram_name,
                    const std::optional<uint64_t> hats_survey_ukm_id,
                    const GURL& hats_survey_url_,
                    const base::TimeDelta& timeout,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data);

  class HatsWebView;

  // Returns the URL for the HaTS wrapper website, with the appropriate query
  // parameters to request the triggered survey.
  GURL GetParameterizedHatsURL() const;

  // Called by |loading_timer_| after |timeout_| time has passed without getting
  // an appropriate response from the HaTS service after creating the widget.
  void LoadTimedOut();

  // Fired by the observer when the survey page has pushed state to the window
  // via URL fragments.
  void OnSurveyStateUpdateReceived(std::string state);

  // Provides mechanism to override URL requested by the dialog. Must be called
  // before CreateWebDialog() to take effect.
  void SetHatsSurveyURLforTesting(GURL url);

  // Displays the widget to the user, called when the dialog believes a survey
  // ready for display. Virtual to allow mocking in tests.
  virtual void ShowWidget();

  // Called by the dialog to close the widget due to timeout or the survey being
  // closed. After the widget is closed, both the widget and this class are
  // destroyed. Virtual to allow mocking in tests.
  virtual void CloseWidget();

  // Returns whether the dialog is still waiting for the survey to load.
  bool IsWaitingForSurveyForTesting();

  // Returns the UMA histogram bucket for a question/answer combination.
  int GetHistogramBucket(int question, int answer);

 private:
  // A timer to prevent unresponsive loading of survey dialog.
  base::OneShotTimer loading_timer_;

  // The off-the-record profile used for browsing to the Chrome HaTS webpage.
  raw_ptr<Profile> otr_profile_;

  raw_ptr<Browser> browser_;

  // The HaTS Next survey trigger ID that is provided to the HaTS webpage.
  const std::string trigger_id_;

  // The UMA histogram name associated with the HaTS survey.
  const std::optional<std::string> hats_histogram_name_;

  // The UKM id associated with the HaTS survey.
  const std::optional<uint64_t> hats_survey_ukm_id_;

  // Whether the web contents has communicated a loaded state.
  bool received_survey_loaded_ = false;

  // The maximum size of the dialog should never exceed the dummy window size
  // provided to the HaTS library by the wrapper website. This is defined
  // in the website source at google3/chrome/hats/website/www/index.html. The
  // minimum size is set at an arbitrary non-zero size as creation of zero sized
  // windows is disallowed on OSX.
  static constexpr gfx::Size kMinSize = gfx::Size(10, 10);
  static constexpr gfx::Size kMaxSize = gfx::Size(800, 600);

  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;

  GURL hats_survey_url_;

  base::TimeDelta timeout_;

  base::OnceClosure success_callback_;
  base::OnceClosure failure_callback_;

  SurveyBitsData product_specific_bits_data_;
  SurveyStringData product_specific_string_data_;

  ukm::builders::Feedback_HappinessTrackingSurvey ukm_hats_builder_;

  base::WeakPtrFactory<HatsNextWebDialog> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_NEXT_WEB_DIALOG_H_
