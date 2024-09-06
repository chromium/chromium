// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/hats/hats_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

constexpr gfx::Size HatsNextWebDialog::kMinSize;
constexpr gfx::Size HatsNextWebDialog::kMaxSize;
constexpr char kSurveyQuestionAnsweredRegex[] = "answer-(\\d+)-((?:\\d+,?)+)";
constexpr char kSurveyQuestionAnsweredAnswerRegex[] = "(\\d+),?";
constexpr char kHatsSurveyCompletedHistogram[] =
    "Feedback.HappinessTrackingSurvey.SurveyCompleted";

void LogUmaHistogramSparse(
    const std::optional<std::string>& hats_histogram_name,
    int enumeration) {
  if (hats_histogram_name.has_value()) {
    base::UmaHistogramSparse(hats_histogram_name.value(), enumeration);
  }
}

void LogUmaHistogramSparse(
    const std::optional<std::string>& hats_histogram_name,
    HatsNextWebDialog::SurveyHistogramEnumeration enumeration) {
  return LogUmaHistogramSparse(hats_histogram_name,
                               static_cast<int>(enumeration));
}

// WebView which contains the WebContents displaying the HaTS Next survey.
class HatsNextWebDialog::HatsWebView : public views::WebView {
  METADATA_HEADER(HatsWebView, views::WebView)

 public:
  HatsWebView(content::BrowserContext* browser_context,
              Browser* browser,
              HatsNextWebDialog* dialog)
      : views::WebView(browser_context), dialog_(dialog), browser_(browser) {}

  ~HatsWebView() override = default;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    // Ignores context menu.
    return true;
  }
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override {
    return true;
  }
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) override {
    // The HaTS Next WebDialog runs with a non-primary OTR profile. This profile
    // cannot open new browser windows, so they are instead opened in the
    // regular browser that initiated the HaTS survey.
    content::OpenURLParams params =
        content::OpenURLParams(target_url, content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false);

    // For the case where we are showing a survey in an undocked DevTools
    // window, we open the URL in the browser of the inspected page.
    if (browser_->is_type_devtools()) {
      DevToolsWindow* devtools_window =
          DevToolsWindow::AsDevToolsWindow(browser_);
      DCHECK(devtools_window);
      devtools_window->OpenURLFromInspectedTab(params);
    } else {
      browser_->OpenURL(params, /*navigation_handle_callback=*/{});
    }
    return nullptr;
  }

  // TODO(crbug.com/40285934): Remove this whole function after HaTSWebUI is
  // launched.
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument() &&
        navigation_handle->IsRendererInitiated()) {
      dialog_->OnSurveyStateUpdateReceived(navigation_handle->GetURL().ref());
    }
  }

 private:
  raw_ptr<HatsNextWebDialog> dialog_;
  raw_ptr<Browser> browser_;
};

BEGIN_METADATA(HatsNextWebDialog, HatsWebView)
END_METADATA

HatsNextWebDialog::HatsNextWebDialog(
    Browser* browser,
    const std::string& trigger_id,
    const std::optional<std::string>& hats_histogram_name,
    const std::optional<uint64_t> hats_survey_ukm_id,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data)
    : HatsNextWebDialog(
          browser,
          trigger_id,
          hats_histogram_name,
          hats_survey_ukm_id,
          base::FeatureList::IsEnabled(features::kHaTSWebUI)
              ? GURL(chrome::kChromeUIUntrustedHatsURL)
              : GURL(features::kHappinessTrackingSurveysHostedUrl.Get()),
          base::Seconds(10),
          std::move(success_callback),
          std::move(failure_callback),
          product_specific_bits_data,
          product_specific_string_data) {}

gfx::Size HatsNextWebDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size preferred_size =
      views::View::CalculatePreferredSize(available_size);
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void HatsNextWebDialog::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile, otr_profile_);
  otr_profile_ = nullptr;
}

std::string HatsNextWebDialog::GetTriggerId() {
  return trigger_id_;
}

bool HatsNextWebDialog::GetEnableTesting() {
  return base::FeatureList::IsEnabled(
      features::kHappinessTrackingSurveysForDesktopDemo);
}

std::vector<std::string> HatsNextWebDialog::GetLanguageList() {
  // The HaTS backend service accepts a list of preferred languages, although
  // only the application locale is provided here to ensure that the survey
  // matches the native UI language.
  return std::vector<std::string>({g_browser_process->GetApplicationLocale()});
}

base::Value::Dict HatsNextWebDialog::GetProductSpecificDataJson() {
  // Append any Product Specific Data to the query. This will be interpreted
  // by the wrapper website and provided to the HaTS backend service.
  base::Value::Dict dict;
  for (const auto& field_value : product_specific_bits_data_) {
    dict.Set(field_value.first, field_value.second ? "true" : "false");
  }
  for (const auto& field_value : product_specific_string_data_) {
    dict.Set(field_value.first, field_value.second);
  }
  return dict;
}

std::optional<std::string> HatsNextWebDialog::GetHistogramName() {
  return hats_histogram_name_;
}

void HatsNextWebDialog::OnSurveyLoaded() {
  // If this has already been called, do nothing. This makes the code robust,
  // should it be called multiple times.
  if (received_survey_loaded_) {
    return;
  }
  loading_timer_.Stop();
  // Record that the survey was shown, and display the widget.
  auto* service = HatsServiceFactory::GetForProfile(browser_->profile(), false);
  DCHECK(service);
  service->RecordSurveyAsShown(trigger_id_);
  received_survey_loaded_ = true;
  ShowWidget();
  LogUmaHistogramSparse(hats_histogram_name_,
                        SurveyHistogramEnumeration::kSurveyLoadedEnumeration);
  if (hats_survey_ukm_id_.has_value()) {
    ukm_hats_builder_.SetSurveyLoaded(true);
  }
  std::move(success_callback_).Run();
}

void HatsNextWebDialog::OnSurveyCompleted() {
  base::UmaHistogramBoolean(kHatsSurveyCompletedHistogram, true);
  LogUmaHistogramSparse(
      hats_histogram_name_,
      SurveyHistogramEnumeration::kSurveyCompletedEnumeration);
  if (hats_survey_ukm_id_.has_value()) {
    ukm_hats_builder_.SetSurveyCompleted(true);
  }
}

void HatsNextWebDialog::OnSurveyClosed() {
  loading_timer_.Stop();
  if (!received_survey_loaded_) {
    // Receiving a close state prior to a loaded state indicates that contact
    // was made with the HaTS Next service, but the HaTS service declined the
    // survey request. This is likely because of a survey misconfiguration,
    // such as a survey still being in test mode, or an invalid survey ID.
    base::UmaHistogramEnumeration(
        kHatsShouldShowSurveyReasonHistogram,
        HatsServiceDesktop::ShouldShowSurveyReasons::kNoRejectedByHatsService);
    std::move(failure_callback_).Run();
  }
  if (hats_survey_ukm_id_.has_value()) {
    ukm_hats_builder_.Record(ukm::UkmRecorder::Get());
  }
  CloseWidget();
}

void HatsNextWebDialog::OnSurveyQuestionAnswered(const std::string& state) {
  if (!hats_histogram_name_.has_value() && !hats_survey_ukm_id_.has_value()) {
    return;
  }

  int question;
  std::vector<int> question_answers;
  if (!ParseSurveyQuestionAnswer(state, &question, &question_answers)) {
    LogUmaHistogramSparse(
        hats_histogram_name_,
        SurveyHistogramEnumeration::kSurveyQuestionAnswerParseError);
    return;
  }

  if (hats_survey_ukm_id_.has_value()) {
    uint64_t ukm_value = EncodeUkmQuestionAnswers(question_answers);

    switch (question) {
      case 1:
        ukm_hats_builder_.SetSurveyAnswerToQuestion1(ukm_value);
        break;
      case 2:
        ukm_hats_builder_.SetSurveyAnswerToQuestion2(ukm_value);
        break;
      case 3:
        ukm_hats_builder_.SetSurveyAnswerToQuestion3(ukm_value);
        break;
      case 4:
        ukm_hats_builder_.SetSurveyAnswerToQuestion4(ukm_value);
        break;
      case 5:
        ukm_hats_builder_.SetSurveyAnswerToQuestion5(ukm_value);
        break;
      case 6:
        ukm_hats_builder_.SetSurveyAnswerToQuestion6(ukm_value);
        break;
      case 7:
        ukm_hats_builder_.SetSurveyAnswerToQuestion7(ukm_value);
        break;
      case 8:
        ukm_hats_builder_.SetSurveyAnswerToQuestion8(ukm_value);
        break;
      case 9:
        ukm_hats_builder_.SetSurveyAnswerToQuestion9(ukm_value);
        break;
    }
  }

  if (hats_histogram_name_.has_value()) {
    for (int answer : question_answers) {
      LogUmaHistogramSparse(hats_histogram_name_,
                            GetHistogramBucket(question, answer));
    }
  }
}

// static
bool HatsNextWebDialog::ParseSurveyQuestionAnswer(const std::string& input,
                                                  int* question,
                                                  std::vector<int>* answers) {
  std::string question_num_string;
  re2::StringPiece all_answers_string;
  if (!RE2::FullMatch(input, kSurveyQuestionAnsweredRegex, &question_num_string,
                      &all_answers_string)) {
    return false;
  }

  if (!base::StringToInt(question_num_string, question) || *question <= 0 ||
      *question > 10) {
    return false;
  }

  std::string answer_string;
  while (RE2::FindAndConsume(&all_answers_string,
                             kSurveyQuestionAnsweredAnswerRegex,
                             &answer_string)) {
    int answer;
    if (!base::StringToInt(answer_string, &answer) || answer <= 0 ||
        answer > 100) {
      return false;
    }
    answers->push_back(answer);
  }

  return true;
}

// static
uint64_t HatsNextWebDialog::EncodeUkmQuestionAnswers(
    const std::vector<int>& question_answers) {
  uint64_t ukm_value = 0;
  for (int answer : question_answers) {
    if (answer > 0) {
      ukm_value |= 1 << (answer - 1);
    }
  }
  return ukm_value;
}

HatsNextWebDialog::HatsNextWebDialog(
    Browser* browser,
    const std::string& trigger_id,
    const std::optional<std::string>& hats_histogram_name,
    const std::optional<uint64_t> hats_survey_ukm_id,
    const GURL& hats_survey_url,
    const base::TimeDelta& timeout,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data)
    : BubbleDialogDelegateView(
          browser->is_type_devtools()
              ? static_cast<views::View*>(
                    BrowserView::GetBrowserViewForBrowser(browser)
                        ->top_container())
              : BrowserView::GetBrowserViewForBrowser(browser)
                    ->toolbar_button_provider()
                    ->GetAppMenuButton(),
          views::BubbleBorder::TOP_RIGHT,
          views::BubbleBorder::DIALOG_SHADOW,
          /*autosize=*/true),
      otr_profile_(browser->profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUnique("HaTSNext:WebDialog"),
          /*create_if_needed=*/true)),
      browser_(browser),
      trigger_id_(trigger_id),
      hats_histogram_name_(
          hats::SurveyConfig::ValidateHatsHistogramName(hats_histogram_name)),
      hats_survey_ukm_id_(
          hats::SurveyConfig::ValidateHatsSurveyUkmId(hats_survey_ukm_id)),
      hats_survey_url_(hats_survey_url),
      timeout_(timeout),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)),
      product_specific_bits_data_(product_specific_bits_data),
      product_specific_string_data_(product_specific_string_data),
      ukm_hats_builder_(browser->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetPrimaryMainFrame()
                            ->GetPageUkmSourceId()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  otr_profile_->AddObserver(this);
  set_close_on_deactivate(false);

  // Override the default zoom level for ths HaTS dialog. Its size should align
  // with native UI elements, rather than web content.
  content::HostZoomMap::GetDefaultForBrowserContext(otr_profile_)
      ->SetZoomLevelForHost(hats_survey_url_.host(),
                            blink::ZoomFactorToZoomLevel(1.0f));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_ =
      AddChildView(std::make_unique<HatsWebView>(otr_profile_, browser, this));
  if (base::FeatureList::IsEnabled(features::kHaTSWebUI)) {
    web_view_->LoadInitialURL(hats_survey_url_);
    web_view_->web_contents()
        ->GetWebUI()
        ->GetController()
        ->GetAs<HatsUI>()
        ->SetHatsPageHandlerDelegate(this);
  } else {
    web_view_->LoadInitialURL(GetParameterizedHatsURL());
  }
  web_view_->EnableSizingFromWebContents(kMinSize, kMaxSize);

  set_margins(gfx::Insets());
  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);

  if (hats_survey_ukm_id_.has_value()) {
    ukm_hats_builder_.SetSurveyId(hats_survey_ukm_id_.value());
    ukm_hats_builder_.SetSurveyLoaded(false);
    ukm_hats_builder_.SetSurveyCompleted(false);
  }

  loading_timer_.Start(FROM_HERE, timeout_,
                       base::BindOnce(&HatsNextWebDialog::LoadTimedOut,
                                      weak_factory_.GetWeakPtr()));
}

HatsNextWebDialog::~HatsNextWebDialog() {
#if IS_ANDROID
  NOTIMPLEMENTED();  // This class is for desktop only. Enforce assumption.
#endif
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (otr_profile_) {
    otr_profile_->RemoveObserver(this);
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile_);
  }
  HatsServiceDesktop* service = static_cast<HatsServiceDesktop*>(
      HatsServiceFactory::GetForProfile(browser_->profile(), false));
  DCHECK(service);
  service->HatsNextDialogClosed();

  // Explicitly clear the delegate to ensure it is not invalid between now and
  // when the web contents is destroyed in the base class.
  web_view_->web_contents()->SetDelegate(nullptr);
}

// TODO(crbug.com/40285934): Remove this whole function after HaTSWebUI is
// launched.
GURL HatsNextWebDialog::GetParameterizedHatsURL() const {
  GURL param_url =
      net::AppendQueryParameter(hats_survey_url_, "trigger_id", trigger_id_);

  // Append any Product Specific Data to the query. This will be interpreted
  // by the wrapper website and provided to the HaTS backend service.
  base::Value::Dict dict;
  for (const auto& field_value : product_specific_bits_data_)
    dict.Set(field_value.first, field_value.second ? "true" : "false");
  for (const auto& field_value : product_specific_string_data_)
    dict.Set(field_value.first, field_value.second);

  std::string product_specific_data_json;
  base::JSONWriter::Write(dict, &product_specific_data_json);

  param_url = net::AppendQueryParameter(param_url, "product_specific_data",
                                        product_specific_data_json);

  // The HaTS backend service accepts a list of preferred languages, although
  // only the application locale is provided here to ensure that the survey
  // matches the native UI language.
  base::Value::List language_list;
  language_list.Append(g_browser_process->GetApplicationLocale());

  std::string language_list_json;
  base::JSONWriter::Write(language_list, &language_list_json);
  param_url =
      net::AppendQueryParameter(param_url, "languages", language_list_json);

  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    param_url = net::AppendQueryParameter(param_url, "enable_testing", "true");
  }

  return param_url;
}

void HatsNextWebDialog::LoadTimedOut() {
  base::UmaHistogramEnumeration(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoSurveyUnreachable);
  CloseWidget();
  std::move(failure_callback_).Run();
}

// TODO(crbug.com/40285934): Remove this whole function after HaTSWebUI is
// launched.
void HatsNextWebDialog::OnSurveyStateUpdateReceived(std::string state) {
  loading_timer_.AbandonAndStop();

  if (state == "loaded") {
    OnSurveyLoaded();
  } else if (state == "close") {
    OnSurveyClosed();
  } else if (state == "completed") {
    OnSurveyCompleted();
  } else if (base::StartsWith(state, "answer-")) {
    OnSurveyQuestionAnswered(state);
  } else {
    LOG(ERROR) << "Unknown state provided in URL fragment by HaTS survey:"
               << state;
    CloseWidget();
    LogUmaHistogramSparse(hats_histogram_name_,
                          SurveyHistogramEnumeration::kSurveyUnknownState);
    std::move(failure_callback_).Run();
  }
}

void HatsNextWebDialog::SetHatsSurveyURLforTesting(GURL url) {
  hats_survey_url_ = url;
}

void HatsNextWebDialog::ShowWidget() {
  widget_->Show();
}

void HatsNextWebDialog::CloseWidget() {
  widget_->Close();
}

bool HatsNextWebDialog::IsWaitingForSurveyForTesting() {
  return loading_timer_.IsRunning();
}

int HatsNextWebDialog::GetHistogramBucket(int question, int answer) {
  // The enumeration is specified as `QQNN`, where `QQ` is the question
  // number and `NN` is the answer index. Therefore, we can calculate this
  // value via `QQ * 100 + NN`.
  // Note: The `ParseSurveyQuestionAnswer` function guarantees that the answer
  // will be in the range [1, 100].
  // The results returned from this function should be consistent with the enum,
  // HappinessTrackingSurvey, which is defined in the file
  // tools/metrics/histograms/metadata/others/enums.xml.
  return question * 100 + answer;
}

BEGIN_METADATA(HatsNextWebDialog)
ADD_READONLY_PROPERTY_METADATA(GURL, ParameterizedHatsURL)
END_METADATA
