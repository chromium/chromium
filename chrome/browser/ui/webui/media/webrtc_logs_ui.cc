// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/upload_list/upload_list.h"
#include "components/version_info/version_info.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateWebRtcLogsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIWebRtcLogsHost);

  source->AddLocalizedString("webrtcLogsTitle", IDS_WEBRTC_LOGS_TITLE);
  source->AddLocalizedString("webrtcTextLogCountFormat",
                             IDS_WEBRTC_TEXT_LOGS_LOG_COUNT_BANNER_FORMAT);
  source->AddLocalizedString("webrtcEventLogCountFormat",
                             IDS_WEBRTC_EVENT_LOGS_LOG_COUNT_BANNER_FORMAT);
  source->AddLocalizedString("webrtcLogHeaderFormat",
                             IDS_WEBRTC_LOGS_LOG_HEADER_FORMAT);
  source->AddLocalizedString("webrtcLogLocalFileLabelFormat",
                             IDS_WEBRTC_LOGS_LOG_LOCAL_FILE_LABEL_FORMAT);
  source->AddLocalizedString("noLocalLogFileMessage",
                             IDS_WEBRTC_LOGS_NO_LOCAL_LOG_FILE_MESSAGE);
  source->AddLocalizedString("webrtcLogUploadTimeFormat",
                             IDS_WEBRTC_LOGS_LOG_UPLOAD_TIME_FORMAT);
  source->AddLocalizedString("webrtcLogFailedUploadTimeFormat",
                             IDS_WEBRTC_LOGS_LOG_FAILED_UPLOAD_TIME_FORMAT);
  source->AddLocalizedString("webrtcLogReportIdFormat",
                             IDS_WEBRTC_LOGS_LOG_REPORT_ID_FORMAT);
  source->AddLocalizedString("webrtcEventLogLocalLogIdFormat",
                             IDS_WEBRTC_LOGS_EVENT_LOG_LOCAL_LOG_ID);
  source->AddLocalizedString("bugLinkText", IDS_WEBRTC_LOGS_BUG_LINK_LABEL);
  source->AddLocalizedString("webrtcLogNotUploadedMessage",
                             IDS_WEBRTC_LOGS_LOG_NOT_UPLOADED_MESSAGE);
  source->AddLocalizedString("webrtcLogPendingMessage",
                             IDS_WEBRTC_LOGS_LOG_PENDING_MESSAGE);
  source->AddLocalizedString("webrtcLogActivelyUploadedMessage",
                             IDS_WEBRTC_LOGS_LOG_ACTIVELY_UPLOADED_MESSAGE);
  source->AddLocalizedString("noTextLogsMessage",
                             IDS_WEBRTC_LOGS_NO_TEXT_LOGS_MESSAGE);
  source->AddLocalizedString("noEventLogsMessage",
                             IDS_WEBRTC_LOGS_NO_EVENT_LOGS_MESSAGE);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("webrtc_logs.js", IDR_WEBRTC_LOGS_JS);
  source->SetDefaultResource(IDR_WEBRTC_LOGS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// WebRtcLogsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://webrtc-logs/ page.
class WebRtcLogsDOMHandler final : public WebUIMessageHandler {
 public:
  explicit WebRtcLogsDOMHandler(Profile* profile);
  ~WebRtcLogsDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  using WebRtcEventLogManager = webrtc_event_logging::WebRtcEventLogManager;

  // Asynchronously fetches the list of upload WebRTC logs. Called from JS.
  void HandleRequestWebRtcLogs(const base::ListValue* args);

  // Asynchronously load WebRTC text logs.
  void LoadWebRtcTextLogs();

  // Callback for when WebRTC text logs have been asynchronously loaded.
  void OnWebRtcTextLogsLoaded();

  // Asynchronously load WebRTC event logs.
  void LoadWebRtcEventLogs();

  // Callback for when WebRTC event logs have been asynchronously loaded.
  void OnWebRtcEventLogsLoaded(
      const std::vector<UploadList::UploadInfo>& event_logs);

  // Update the chrome://webrtc-logs/ page.
  void UpdateUI();

  // Update the text/event logs part of the forementioned page.
  void UpdateUIWithTextLogs(base::ListValue* text_logs_list) const;
  void UpdateUIWithEventLogs(base::ListValue* event_logs_list) const;

  // Convert a history entry about a captured WebRTC event log into a
  // DictionaryValue of the type expected by updateWebRtcLogsList().
  std::unique_ptr<base::DictionaryValue> EventLogUploadInfoToDictionaryValue(
      const UploadList::UploadInfo& info) const;

  // Helpers for EventLogUploadInfoToDictionaryValue().
  std::unique_ptr<base::DictionaryValue> FromPendingLog(
      const UploadList::UploadInfo& info) const;
  std::unique_ptr<base::DictionaryValue> FromActivelyUploadedLog(
      const UploadList::UploadInfo& info) const;
  std::unique_ptr<base::DictionaryValue> FromNotUploadedLog(
      const UploadList::UploadInfo& info) const;
  std::unique_ptr<base::DictionaryValue> FromUploadUnsuccessfulLog(
      const UploadList::UploadInfo& info) const;
  std::unique_ptr<base::DictionaryValue> FromUploadSuccessfulLog(
      const UploadList::UploadInfo& info) const;

  bool SanityCheckOnUploadInfo(const UploadList::UploadInfo& info) const;

  // The directories where the (text/event) logs are stored.
  const base::FilePath text_log_dir_;
  const base::FilePath event_log_dir_;

  // Identifies to WebRtcEventLogManager the profile with which |this| is
  // associated. Technically, we should be able to just keep Profile*,
  // but avoiding it makes less lifetime assumptions.
  // Note that the profile->GetOriginalProfile() is used, to make sure that
  // for incognito profiles, the parent profile's event logs are shown,
  // as is the behavior for text logs.
  const WebRtcEventLogManager::BrowserContextId original_browser_context_id_;

  // Loads, parses and stores the list of uploaded text WebRTC logs.
  scoped_refptr<UploadList> text_log_upload_list_;

  // List of WebRTC logs captured and possibly uploaded to Crash.
  std::vector<UploadList::UploadInfo> event_logs_;

  // Factory for creating weak references to instances of this class.
  base::WeakPtrFactory<WebRtcLogsDOMHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLogsDOMHandler);
};

WebRtcLogsDOMHandler::WebRtcLogsDOMHandler(Profile* profile)
    : text_log_dir_(
          webrtc_logging::TextLogList::
              GetWebRtcLogDirectoryForBrowserContextPath(profile->GetPath())),
      event_log_dir_(
          WebRtcEventLogManager::GetRemoteBoundWebRtcEventLogsDir(profile)),
      original_browser_context_id_(webrtc_event_logging::GetBrowserContextId(
          profile->GetOriginalProfile())),
      text_log_upload_list_(
          webrtc_logging::TextLogList::CreateWebRtcLogList(profile)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

WebRtcLogsDOMHandler::~WebRtcLogsDOMHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  text_log_upload_list_->CancelCallback();
}

void WebRtcLogsDOMHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "requestWebRtcLogsList",
      base::BindRepeating(&WebRtcLogsDOMHandler::HandleRequestWebRtcLogs,
                          weak_ptr_factory_.GetWeakPtr()));
}

void WebRtcLogsDOMHandler::HandleRequestWebRtcLogs(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LoadWebRtcTextLogs();
}

void WebRtcLogsDOMHandler::LoadWebRtcTextLogs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  text_log_upload_list_->Load(
      base::BindOnce(&WebRtcLogsDOMHandler::OnWebRtcTextLogsLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebRtcLogsDOMHandler::OnWebRtcTextLogsLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LoadWebRtcEventLogs();  // Text logs loaded; on to the event logs.
}

void WebRtcLogsDOMHandler::LoadWebRtcEventLogs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  WebRtcEventLogManager* manager = WebRtcEventLogManager::GetInstance();
  if (manager) {
    manager->GetHistory(
        original_browser_context_id_,
        base::BindOnce(&WebRtcLogsDOMHandler::OnWebRtcEventLogsLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnWebRtcEventLogsLoaded(std::vector<UploadList::UploadInfo>());
  }
}

void WebRtcLogsDOMHandler::OnWebRtcEventLogsLoaded(
    const std::vector<UploadList::UploadInfo>& event_logs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  event_logs_ = event_logs;

  UpdateUI();  // All log histories loaded asynchronously; time to display.
}

void WebRtcLogsDOMHandler::UpdateUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ListValue text_logs_list;
  UpdateUIWithTextLogs(&text_logs_list);

  base::ListValue event_logs_list;
  UpdateUIWithEventLogs(&event_logs_list);

  base::Value version(version_info::GetVersionNumber());

  web_ui()->CallJavascriptFunctionUnsafe("updateWebRtcLogsList", text_logs_list,
                                         event_logs_list, version);
}

void WebRtcLogsDOMHandler::UpdateUIWithTextLogs(
    base::ListValue* upload_list) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<UploadList::UploadInfo> uploads;
  text_log_upload_list_->GetUploads(50, &uploads);

  for (auto i = uploads.begin(); i != uploads.end(); ++i) {
    std::unique_ptr<base::DictionaryValue> upload(new base::DictionaryValue());
    upload->SetString("id", i->upload_id);

    base::string16 value_w;
    if (!i->upload_time.is_null())
      value_w = base::TimeFormatFriendlyDateAndTime(i->upload_time);
    upload->SetString("upload_time", value_w);

    base::FilePath::StringType value;
    if (!i->local_id.empty())
      value = text_log_dir_.AppendASCII(i->local_id)
                  .AddExtension(FILE_PATH_LITERAL(".gz"))
                  .value();
    upload->SetString("local_file", value);

    // In october 2015, capture time was added to the log list, previously the
    // local ID was used as capture time. The local ID has however changed so
    // that it might not be a time. We fall back on the local ID if it traslates
    // to a time within reasonable bounds, otherwise we fall back on the upload
    // time.
    // TODO(grunell): Use |capture_time| only.
    if (!i->capture_time.is_null()) {
      value_w = base::TimeFormatFriendlyDateAndTime(i->capture_time);
    } else {
      // Fall back on local ID as time. We need to check that it's within
      // resonable bounds, since the ID may not represent time. Check between
      // 2012 when the feature was introduced and now.
      double seconds_since_epoch;
      if (base::StringToDouble(i->local_id, &seconds_since_epoch)) {
        base::Time capture_time = base::Time::FromDoubleT(seconds_since_epoch);
        const base::Time::Exploded lower_limit = {2012, 1, 0, 1, 0, 0, 0, 0};
        base::Time out_time;
        bool conversion_success =
            base::Time::FromUTCExploded(lower_limit, &out_time);
        DCHECK(conversion_success);
        if (capture_time > out_time && capture_time < base::Time::Now()) {
          value_w = base::TimeFormatFriendlyDateAndTime(capture_time);
        }
      }
    }
    // If we haven't set |value_w| above, we fall back on the upload time, which
    // was already in the variable. In case it's empty set the string to
    // inform that the time is unknown.
    if (value_w.empty())
      value_w = base::string16(base::ASCIIToUTF16("(unknown time)"));
    upload->SetString("capture_time", value_w);

    upload_list->Append(std::move(upload));
  }
}

void WebRtcLogsDOMHandler::UpdateUIWithEventLogs(
    base::ListValue* event_logs_list) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto it = event_logs_.crbegin(); it != event_logs_.crend(); ++it) {
    event_logs_list->Append(EventLogUploadInfoToDictionaryValue(*it));
  }
}

std::unique_ptr<base::DictionaryValue>
WebRtcLogsDOMHandler::EventLogUploadInfoToDictionaryValue(
    const UploadList::UploadInfo& info) const {
  switch (info.state) {
    case UploadList::UploadInfo::State::Pending:
      // TODO(crbug.com/775415): Display actively-written logs differently
      // than fully captured pending logs.
      return info.upload_time.is_null() ? FromPendingLog(info)
                                        : FromActivelyUploadedLog(info);
    case UploadList::UploadInfo::State::NotUploaded:
      return info.upload_time.is_null() ? FromNotUploadedLog(info)
                                        : FromUploadUnsuccessfulLog(info);
    case UploadList::UploadInfo::State::Uploaded:
      return FromUploadSuccessfulLog(info);
    case UploadList::UploadInfo::State::Pending_UserRequested:
      NOTREACHED();
  }

  LOG(ERROR) << "Unrecognized state (" << static_cast<int>(info.state) << ").";
  return nullptr;
}

std::unique_ptr<base::DictionaryValue> WebRtcLogsDOMHandler::FromPendingLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::Pending);
  DCHECK(info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> log(new base::DictionaryValue());
  log->SetString("state", "pending");
  log->SetString("capture_time",
                 base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log->SetString("local_file",
                 event_log_dir_.AppendASCII(info.local_id).value());
  return log;
}

std::unique_ptr<base::DictionaryValue>
WebRtcLogsDOMHandler::FromActivelyUploadedLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::Pending);
  DCHECK(!info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> log(new base::DictionaryValue());
  log->SetString("state", "actively_uploaded");
  log->SetString("capture_time",
                 base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log->SetString("local_file",
                 event_log_dir_.AppendASCII(info.local_id).value());
  return log;
}

std::unique_ptr<base::DictionaryValue> WebRtcLogsDOMHandler::FromNotUploadedLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::NotUploaded);
  DCHECK(info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> log(new base::DictionaryValue());
  log->SetString("state", "not_uploaded");
  log->SetString("capture_time",
                 base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log->SetString("local_id", info.local_id);
  return log;
}

std::unique_ptr<base::DictionaryValue>
WebRtcLogsDOMHandler::FromUploadUnsuccessfulLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::NotUploaded);
  DCHECK(!info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return nullptr;
  }

  if (!info.upload_id.empty()) {
    LOG(ERROR) << "Unexpected upload ID.";
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> log(new base::DictionaryValue());
  log->SetString("state", "upload_unsuccessful");
  log->SetString("capture_time",
                 base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log->SetString("local_id", info.local_id);
  log->SetString("upload_time",
                 base::TimeFormatFriendlyDateAndTime(info.upload_time));
  return log;
}

std::unique_ptr<base::DictionaryValue>
WebRtcLogsDOMHandler::FromUploadSuccessfulLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::Uploaded);
  DCHECK(!info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return nullptr;
  }

  if (info.upload_id.empty()) {
    LOG(ERROR) << "Unknown upload ID.";
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> log(new base::DictionaryValue());
  log->SetString("state", "upload_successful");
  log->SetString("capture_time",
                 base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log->SetString("local_id", info.local_id);
  log->SetString("upload_id", info.upload_id);
  log->SetString("upload_time",
                 base::TimeFormatFriendlyDateAndTime(info.upload_time));
  return log;
}

bool WebRtcLogsDOMHandler::SanityCheckOnUploadInfo(
    const UploadList::UploadInfo& info) const {
  if (info.capture_time.is_null()) {
    LOG(ERROR) << "Unknown capture time.";
    return false;
  }

  if (info.local_id.empty()) {
    LOG(ERROR) << "Unknown local ID.";
    return false;
  }

  return true;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// WebRtcLogsUI
//
///////////////////////////////////////////////////////////////////////////////

WebRtcLogsUI::WebRtcLogsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<WebRtcLogsDOMHandler>(profile));

  // Set up the chrome://webrtc-logs/ source.
  content::WebUIDataSource::Add(profile, CreateWebRtcLogsUIHTMLSource());
}
