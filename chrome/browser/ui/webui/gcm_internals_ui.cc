// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gcm_internals_ui.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_internals_constants.h"
#include "components/gcm_driver/gcm_internals_helper.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/grit/dev_ui_components_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

GCMInternalsUIConfig::GCMInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIGCMInternalsHost) {}

namespace {

// Class acting as a controller of the chrome://gcm-internals WebUI.
class GcmInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  GcmInternalsUIMessageHandler();

  GcmInternalsUIMessageHandler(const GcmInternalsUIMessageHandler&) = delete;
  GcmInternalsUIMessageHandler& operator=(const GcmInternalsUIMessageHandler&) =
      delete;

  ~GcmInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  // Return all of the GCM related infos to the gcm-internals page by calling
  // Javascript callback function
  // |gcm-internals.returnInfo()|.
  void ReturnResults(Profile* profile,
                     gcm::GCMProfileService* profile_service,
                     const gcm::GCMClient::GCMStatistics* stats);

  // Request all of the GCM related infos through gcm profile service.
  void RequestAllInfo(const base::Value::List& args);

  // Enables/disables GCM activity recording through gcm profile service.
  void SetRecording(const base::Value::List& args);

  // Callback function of the request for all gcm related infos.
  void RequestGCMStatisticsFinished(const gcm::GCMClient::GCMStatistics& args);

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GcmInternalsUIMessageHandler> weak_ptr_factory_{this};
};

GcmInternalsUIMessageHandler::GcmInternalsUIMessageHandler() {}

GcmInternalsUIMessageHandler::~GcmInternalsUIMessageHandler() {}

void GcmInternalsUIMessageHandler::ReturnResults(
    Profile* profile,
    gcm::GCMProfileService* profile_service,
    const gcm::GCMClient::GCMStatistics* stats) {
  base::Value::Dict results = gcm_driver::SetGCMInternalsInfo(
      stats, profile_service, profile->GetPrefs());
  FireWebUIListener(gcm_driver::kSetGcmInternalsInfo, results);
}

void GcmInternalsUIMessageHandler::RequestAllInfo(
    const base::Value::List& list) {
  AllowJavascript();
  if (list.size() != 1) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const bool clear_logs = list[0].GetBool();

  gcm::GCMDriver::ClearActivityLogs clear_activity_logs =
      clear_logs ? gcm::GCMDriver::CLEAR_LOGS : gcm::GCMDriver::KEEP_LOGS;

  Profile* profile = Profile::FromWebUI(web_ui());
  gcm::GCMProfileService* profile_service =
    gcm::GCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service || !profile_service->driver()) {
    ReturnResults(profile, nullptr, nullptr);
  } else {
    profile_service->driver()->GetGCMStatistics(
        base::BindOnce(
            &GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
            weak_ptr_factory_.GetWeakPtr()),
        clear_activity_logs);
  }
}

void GcmInternalsUIMessageHandler::SetRecording(const base::Value::List& list) {
  if (list.size() != 1) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const bool recording = list[0].GetBool();

  Profile* profile = Profile::FromWebUI(web_ui());
  gcm::GCMProfileService* profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service) {
    ReturnResults(profile, nullptr, nullptr);
    return;
  }
  // Get fresh stats after changing recording setting.
  profile_service->driver()->SetGCMRecording(
      base::BindRepeating(
          &GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
          weak_ptr_factory_.GetWeakPtr()),
      recording);
}

void GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished(
    const gcm::GCMClient::GCMStatistics& stats) {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);
  gcm::GCMProfileService* profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  DCHECK(profile_service);
  ReturnResults(profile, profile_service, &stats);
}

void GcmInternalsUIMessageHandler::RegisterMessages() {
  // It is safe to use base::Unretained here, since web_ui owns this message
  // handler.
  web_ui()->RegisterMessageCallback(
      gcm_driver::kGetGcmInternalsInfo,
      base::BindRepeating(&GcmInternalsUIMessageHandler::RequestAllInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      gcm_driver::kSetGcmInternalsRecording,
      base::BindRepeating(&GcmInternalsUIMessageHandler::SetRecording,
                          base::Unretained(this)));
}

void GcmInternalsUIMessageHandler::OnJavascriptDisallowed() {
  // Invalidate weak ptrs in order to cancel callbacks from the
  // GCMProfileServiceFactory. If the page is being navigated away from, this
  // prevents such callbacks from triggering a CHECK by trying to run JS code
  // on some other page. If the page is refreshed, this prevents the callbacks
  // from triggering a CHECK by trying to run JS code on the refreshed page when
  // the JS side is not yet ready, which can lead to a broken UI experience.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace

GCMInternalsUI::GCMInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gcm-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUIGCMInternalsHost);

  html_source->UseStringsJs();

  // Add required resources.
  html_source->AddResourcePath(gcm_driver::kGcmInternalsCSS,
                               IDR_GCM_DRIVER_GCM_INTERNALS_CSS);
  html_source->AddResourcePath(gcm_driver::kGcmInternalsJS,
                               IDR_GCM_DRIVER_GCM_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_GCM_DRIVER_GCM_INTERNALS_HTML);

  web_ui->AddMessageHandler(std::make_unique<GcmInternalsUIMessageHandler>());
}

GCMInternalsUI::~GCMInternalsUI() {}
