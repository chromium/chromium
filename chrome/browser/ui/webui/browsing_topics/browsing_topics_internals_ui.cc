// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"

#include "base/i18n/time_formatting.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browsing_topics_internals_resources.h"
#include "chrome/grit/browsing_topics_internals_resources_map.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace {

void AddTopicsConsentStrings(content::WebUIDataSource* html_source,
                             Profile* profile) {
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);

  // Regardless of profile type, the service should always exist.
  DCHECK(privacy_sandbox_service);

  int consent_status_string_id = 0;
  if (!privacy_sandbox_service->TopicsConsentRequired()) {
    consent_status_string_id = IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_NOT_REQUIRED;
  } else if (privacy_sandbox_service->TopicsHasActiveConsent()) {
    DCHECK(privacy_sandbox_service->TopicsConsentRequired());
    consent_status_string_id = IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_ACTIVE;
  } else {
    DCHECK(!privacy_sandbox_service->TopicsHasActiveConsent());
    DCHECK(privacy_sandbox_service->TopicsConsentRequired());
    consent_status_string_id = IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_INACTIVE;
  }
  DCHECK(consent_status_string_id);
  html_source->AddLocalizedString("topicsConsentStatus",
                                  consent_status_string_id);

  int consent_source_string_id = 0;
  switch (privacy_sandbox_service->TopicsConsentLastUpdateSource()) {
    case (privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue): {
      consent_source_string_id =
          IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_UPDATE_SOURCE_DEFAULT;
      break;
    }
    case (privacy_sandbox::TopicsConsentUpdateSource::kSettings): {
      consent_source_string_id =
          IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_UPDATE_SOURCE_SETTINGS;
      break;
    }
    case (privacy_sandbox::TopicsConsentUpdateSource::kConfirmation): {
      consent_source_string_id =
          IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_UPDATE_SOURCE_CONFIRMATION;
      break;
    }
  }
  DCHECK(consent_source_string_id);
  html_source->AddLocalizedString("topicsConsentSource",
                                  consent_source_string_id);

  html_source->AddString(
      "topicsConsentTime",
      base::TimeFormatFriendlyDateAndTime(
          privacy_sandbox_service->TopicsConsentLastUpdateTime()));

  html_source->AddString(
      "topicsConsentText",
      privacy_sandbox_service->TopicsConsentLastUpdateText());

  // Static strings
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"topicsConsentStatusLabel",
       IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_STATUS_LABEL},
      {"topicsConsentSourceLabel",
       IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_LAST_UPDATE_SOURCE_LABEL},
      {"topicsConsentTimeLabel",
       IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_LAST_UPDATE_TIME_LABEL},
      {"topicsConsentTextLabel",
       IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_LAST_UPDATE_TEXT_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace

BrowsingTopicsInternalsUI::BrowsingTopicsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIBrowsingTopicsInternalsHost);

  AddTopicsConsentStrings(source, Profile::FromWebUI(web_ui));

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kBrowsingTopicsInternalsResources,
                      kBrowsingTopicsInternalsResourcesSize),
      IDR_BROWSING_TOPICS_INTERNALS_BROWSING_TOPICS_INTERNALS_HTML);
}

BrowsingTopicsInternalsUI::~BrowsingTopicsInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BrowsingTopicsInternalsUI)

void BrowsingTopicsInternalsUI::BindInterface(
    mojo::PendingReceiver<browsing_topics::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<BrowsingTopicsInternalsPageHandler>(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      std::move(receiver));
}
