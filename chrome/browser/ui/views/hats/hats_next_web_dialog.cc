// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/pref_names.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

constexpr gfx::Size HatsNextWebDialog::kMinSize;
constexpr gfx::Size HatsNextWebDialog::kMaxSize;

// WebView which contains the WebContents displaying the HaTS Next survey.
class HatsNextWebDialog::HatsWebView : public views::WebView {
 public:
  METADATA_HEADER(HatsWebView);
  HatsWebView(content::BrowserContext* browser_context,
              Browser* browser,
              HatsNextWebDialog* dialog)
      : views::WebView(browser_context), dialog_(dialog), browser_(browser) {}

  ~HatsWebView() override = default;

  // views::WebView:
  void PreferredSizeChanged() override {
    WebView::PreferredSizeChanged();
    dialog_->UpdateWidgetSize();
  }

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
      browser_->OpenURL(params);
    }
    return nullptr;
  }

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

BEGIN_METADATA(HatsNextWebDialog, HatsWebView, views::WebView)
END_METADATA

HatsNextWebDialog::HatsNextWebDialog(
    Browser* browser,
    const std::string& trigger_id,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data)
    : HatsNextWebDialog(
          browser,
          trigger_id,
          GURL("https://storage.googleapis.com/chrome_hats_staging/index.html"),
          base::Seconds(10),
          std::move(success_callback),
          std::move(failure_callback),
          product_specific_bits_data,
          product_specific_string_data) {}

gfx::Size HatsNextWebDialog::CalculatePreferredSize() const {
  gfx::Size preferred_size = views::View::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void HatsNextWebDialog::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile, otr_profile_);
  otr_profile_ = nullptr;
}

HatsNextWebDialog::HatsNextWebDialog(
    Browser* browser,
    const std::string& trigger_id,
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
          views::BubbleBorder::TOP_RIGHT),
      otr_profile_(browser->profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUnique("HaTSNext:WebDialog"),
          /*create_if_needed=*/true)),
      browser_(browser),
      trigger_id_(trigger_id),
      hats_survey_url_(hats_survey_url),
      timeout_(timeout),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)),
      product_specific_bits_data_(product_specific_bits_data),
      product_specific_string_data_(product_specific_string_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  otr_profile_->AddObserver(this);
  set_close_on_deactivate(false);

  // Override the default zoom level for ths HaTS dialog. Its size should align
  // with native UI elements, rather than web content.
  content::HostZoomMap::GetDefaultForBrowserContext(otr_profile_)
      ->SetZoomLevelForHost(hats_survey_url_.host(),
                            blink::PageZoomFactorToZoomLevel(1.0f));

  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_ =
      AddChildView(std::make_unique<HatsWebView>(otr_profile_, browser, this));
  web_view_->LoadInitialURL(GetParameterizedHatsURL());
  web_view_->EnableSizingFromWebContents(kMinSize, kMaxSize);

  set_margins(gfx::Insets());
  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);

  loading_timer_.Start(FROM_HERE, timeout_,
                       base::BindOnce(&HatsNextWebDialog::LoadTimedOut,
                                      weak_factory_.GetWeakPtr()));
}

HatsNextWebDialog::~HatsNextWebDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (otr_profile_) {
    otr_profile_->RemoveObserver(this);
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile_);
  }
  auto* service = HatsServiceFactory::GetForProfile(browser_->profile(), false);
  DCHECK(service);
  service->HatsNextDialogClosed();

  // Explicitly clear the delegate to ensure it is not invalid between now and
  // when the web contents is destroyed in the base class.
  web_view_->web_contents()->SetDelegate(nullptr);
}

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
      HatsService::ShouldShowSurveyReasons::kNoSurveyUnreachable);
  CloseWidget();
  std::move(failure_callback_).Run();
}

void HatsNextWebDialog::OnSurveyStateUpdateReceived(std::string state) {
  loading_timer_.AbandonAndStop();

  if (state == "loaded") {
    // Record that the survey was shown, and display the widget.
    auto* service =
        HatsServiceFactory::GetForProfile(browser_->profile(), false);
    DCHECK(service);
    service->RecordSurveyAsShown(trigger_id_);
    received_survey_loaded_ = true;
    ShowWidget();
    std::move(success_callback_).Run();
  } else if (state == "close") {
    if (!received_survey_loaded_) {
      // Receiving a close state prior to a loaded state indicates that contact
      // was made with the HaTS Next service, but the HaTS service declined the
      // survey request. This is likely because of a survey misconfiguration,
      // such as a survey still being in test mode, or an invalid survey ID.
      base::UmaHistogramEnumeration(
          kHatsShouldShowSurveyReasonHistogram,
          HatsService::ShouldShowSurveyReasons::kNoRejectedByHatsService);
      std::move(failure_callback_).Run();
    }
    CloseWidget();
  } else {
    LOG(ERROR) << "Unknown state provided in URL fragment by HaTS survey:"
               << state;
    CloseWidget();
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

void HatsNextWebDialog::UpdateWidgetSize() {
  SizeToContents();
}

bool HatsNextWebDialog::IsWaitingForSurveyForTesting() {
  return loading_timer_.IsRunning();
}

BEGIN_METADATA(HatsNextWebDialog, views::BubbleDialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(GURL, ParameterizedHatsURL)
END_METADATA
