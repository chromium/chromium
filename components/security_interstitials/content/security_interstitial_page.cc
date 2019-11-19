// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_page.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace security_interstitials {

SecurityInterstitialPage::SecurityInterstitialPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<SecurityInterstitialControllerClient> controller)
    : web_contents_(web_contents),
      request_url_(request_url),
      interstitial_page_(nullptr),
      create_view_(true),
      on_show_extended_reporting_pref_exists_(false),
      on_show_extended_reporting_pref_value_(false),
      controller_(std::move(controller)) {
  // Determine if any prefs need to be updated prior to showing the security
  // interstitial. Note that some content embedders (such as Android WebView)
  // uses security interstitials without a prefservice.
  if (controller_->GetPrefService()) {
    safe_browsing::UpdatePrefsBeforeSecurityInterstitial(
        controller_->GetPrefService());
  }
  SetUpMetrics();

  // Creating interstitial_page_ without showing it leaks memory, so don't
  // create it here.
}

SecurityInterstitialPage::~SecurityInterstitialPage() {
}

content::InterstitialPage* SecurityInterstitialPage::interstitial_page() const {
  return interstitial_page_;
}

content::WebContents* SecurityInterstitialPage::web_contents() const {
  return web_contents_;
}

GURL SecurityInterstitialPage::request_url() const {
  return request_url_;
}

void SecurityInterstitialPage::DontCreateViewForTesting() {
  create_view_ = false;
}

bool SecurityInterstitialPage::ShouldDisplayURL() const {
  return true;
}

std::string SecurityInterstitialPage::GetHTMLContents() {
  base::DictionaryValue load_time_data;
  PopulateInterstitialStrings(&load_time_data);
  webui::SetLoadTimeDataDefaults(controller()->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          GetHTMLTemplateId());

  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, &load_time_data);
}

void SecurityInterstitialPage::Show() {
  DCHECK(!interstitial_page_);
  interstitial_page_ = content::InterstitialPage::Create(
      web_contents_, ShouldCreateNewNavigation(), request_url_, this);
  if (!create_view_)
    interstitial_page_->DontCreateViewForTesting();

  interstitial_page_->Show();

  controller_->set_interstitial_page(interstitial_page_);
}

SecurityInterstitialControllerClient* SecurityInterstitialPage::controller()
    const {
  return controller_.get();
}

void SecurityInterstitialPage::UpdateMetricsAfterSecurityInterstitial() {
  if (controller_->GetPrefService()) {
    safe_browsing::UpdateMetricsAfterSecurityInterstitial(
        *controller_->GetPrefService(), on_show_extended_reporting_pref_exists_,
        on_show_extended_reporting_pref_value_);
  }
}

void SecurityInterstitialPage::SetUpMetrics() {
  // Remember the initial state of the extended reporting pref, to be compared
  // to the same data when the interstitial is closed.
  PrefService* prefs = controller_->GetPrefService();
  if (prefs) {
    on_show_extended_reporting_pref_exists_ =
        safe_browsing::ExtendedReportingPrefExists(*prefs);
    on_show_extended_reporting_pref_value_ =
        safe_browsing::IsExtendedReportingEnabled(*prefs);
  }
}

base::string16 SecurityInterstitialPage::GetFormattedHostName() const {
  return security_interstitials::common_string_util::GetFormattedHostName(
      request_url_);
}

int SecurityInterstitialPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

}  // security_interstitials
