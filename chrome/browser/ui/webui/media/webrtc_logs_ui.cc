// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/media_resources.h"
#include "components/prefs/pref_service.h"
#include "components/upload_list/upload_list.h"
#include "components/version_info/version_info.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

void CreateAndAddWebRtcLogsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIWebRtcLogsHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"webrtcLogsTitle", IDS_WEBRTC_LOGS_TITLE},
      {"webrtcTextLogCountFormat",
       IDS_WEBRTC_TEXT_LOGS_LOG_COUNT_BANNER_FORMAT},
      {"webrtcEventLogCountFormat",
       IDS_WEBRTC_EVENT_LOGS_LOG_COUNT_BANNER_FORMAT},
      {"webrtcLogHeaderFormat", IDS_WEBRTC_LOGS_LOG_HEADER_FORMAT},
      {"webrtcLogLocalFileLabelFormat",
       IDS_WEBRTC_LOGS_LOG_LOCAL_FILE_LABEL_FORMAT},
      {"noLocalLogFileMessage", IDS_WEBRTC_LOGS_NO_LOCAL_LOG_FILE_MESSAGE},
      {"webrtcLogUploadTimeFormat", IDS_WEBRTC_LOGS_LOG_UPLOAD_TIME_FORMAT},
      {"webrtcLogFailedUploadTimeFormat",
       IDS_WEBRTC_LOGS_LOG_FAILED_UPLOAD_TIME_FORMAT},
      {"webrtcLogReportIdFormat", IDS_WEBRTC_LOGS_LOG_REPORT_ID_FORMAT},
      {"webrtcEventLogLocalLogIdFormat",
       IDS_WEBRTC_LOGS_EVENT_LOG_LOCAL_LOG_ID},
      {"bugLinkText", IDS_WEBRTC_LOGS_BUG_LINK_LABEL},
      {"webrtcLogNotUploadedMessage", IDS_WEBRTC_LOGS_LOG_NOT_UPLOADED_MESSAGE},
      {"webrtcLogPendingMessage", IDS_WEBRTC_LOGS_LOG_PENDING_MESSAGE},
      {"webrtcLogActivelyUploadedMessage",
       IDS_WEBRTC_LOGS_LOG_ACTIVELY_UPLOADED_MESSAGE},
      {"noTextLogsMessage", IDS_WEBRTC_LOGS_NO_TEXT_LOGS_MESSAGE},
      {"noEventLogsMessage", IDS_WEBRTC_LOGS_NO_EVENT_LOGS_MESSAGE},
  };
  source->AddLocalizedStrings(kStrings);

  source->UseStringsJs();

  source->AddResourcePath("webrtc_logs.css", IDR_MEDIA_WEBRTC_LOGS_CSS);
  source->AddResourcePath("webrtc_logs.js", IDR_MEDIA_WEBRTC_LOGS_JS);
  source->SetDefaultResource(IDR_MEDIA_WEBRTC_LOGS_HTML);
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

  WebRtcLogsDOMHandler(const WebRtcLogsDOMHandler&) = delete;
  WebRtcLogsDOMHandler& operator=(const WebRtcLogsDOMHandler&) = delete;

  ~WebRtcLogsDOMHandler() override;

  // WebUIMessageHandler implementation.
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

 private:
  using WebRtcEventLogManager = webrtc_event_logging::WebRtcEventLogManager;

  // Asynchronously fetches the list of upload WebRTC logs. Called from JS.
  void HandleRequestWebRtcLogs(const base::Value::List& args);

  // Asynchronously load WebRTC text logs.
  void LoadWebRtcTextLogs(const std::string& callback_id);

  // Callback for when WebRTC text logs have been asynchronously loaded.
  void OnWebRtcTextLogsLoaded(const std::string& callback_id);

  // Asynchronously load WebRTC event logs.
  void LoadWebRtcEventLogs(const std::string& callback_id);

  // Callback for when WebRTC event logs have been asynchronously loaded.
  void OnWebRtcEventLogsLoaded(
      const std::string& callback_id,
      const std::vector<UploadList::UploadInfo>& event_logs);

  // Update the chrome://webrtc-logs/ page.
  void UpdateUI(const std::string& callback_id);

  // Update the text/event logs part of the forementioned page.
  base::Value::List UpdateUIWithTextLogs() const;
  base::Value::List UpdateUIWithEventLogs() const;

  // Convert a history entry about a captured WebRTC event log into a
  // Value of the type expected by updateWebRtcLogsList().
  base::Value EventLogUploadInfoToValue(
      const UploadList::UploadInfo& info) const;

  // Helpers for `EventLogUploadInfoToValue()`.
  base::Value FromPendingLog(const UploadList::UploadInfo& info) const;
  base::Value FromActivelyUploadedLog(const UploadList::UploadInfo& info) const;
  base::Value FromNotUploadedLog(const UploadList::UploadInfo& info) const;
  base::Value FromUploadUnsuccessfulLog(
      const UploadList::UploadInfo& info) const;
  base::Value FromUploadSuccessfulLog(const UploadList::UploadInfo& info) const;

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
  base::WeakPtrFactory<WebRtcLogsDOMHandler> weak_ptr_factory_{this};
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
          webrtc_logging::TextLogList::CreateWebRtcLogList(profile)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

WebRtcLogsDOMHandler::~WebRtcLogsDOMHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  text_log_upload_list_->CancelLoadCallback();
}

void WebRtcLogsDOMHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "requestWebRtcLogsList",
      base::BindRepeating(&WebRtcLogsDOMHandler::HandleRequestWebRtcLogs,
                          base::Unretained(this)));
}

void WebRtcLogsDOMHandler::HandleRequestWebRtcLogs(
    const base::Value::List& args) {
  std::string callback_id = args[0].GetString();
  AllowJavascript();
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LoadWebRtcTextLogs(callback_id);
}

void WebRtcLogsDOMHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WebRtcLogsDOMHandler::LoadWebRtcTextLogs(const std::string& callback_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  text_log_upload_list_->Load(
      base::BindOnce(&WebRtcLogsDOMHandler::OnWebRtcTextLogsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void WebRtcLogsDOMHandler::OnWebRtcTextLogsLoaded(
    const std::string& callback_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LoadWebRtcEventLogs(callback_id);  // Text logs loaded; on to the event logs.
}

void WebRtcLogsDOMHandler::LoadWebRtcEventLogs(const std::string& callback_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  WebRtcEventLogManager* manager = WebRtcEventLogManager::GetInstance();
  if (manager) {
    manager->GetHistory(
        original_browser_context_id_,
        base::BindOnce(&WebRtcLogsDOMHandler::OnWebRtcEventLogsLoaded,
                       weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    OnWebRtcEventLogsLoaded(callback_id, std::vector<UploadList::UploadInfo>());
  }
}

void WebRtcLogsDOMHandler::OnWebRtcEventLogsLoaded(
    const std::string& callback_id,
    const std::vector<UploadList::UploadInfo>& event_logs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  event_logs_ = event_logs;

  // All log histories loaded asynchronously; time to display.
  UpdateUI(callback_id);
}

void WebRtcLogsDOMHandler::UpdateUI(const std::string& callback_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::Dict result;
  result.Set("textLogs", UpdateUIWithTextLogs());
  result.Set("eventLogs", UpdateUIWithEventLogs());
  result.Set("version", version_info::GetVersionNumber());
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

base::Value::List WebRtcLogsDOMHandler::UpdateUIWithTextLogs() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::List result;
  const std::vector<const UploadList::UploadInfo*> uploads =
      text_log_upload_list_->GetUploads(50);

  for (const auto* upload : uploads) {
    base::Value::Dict upload_value;
    upload_value.Set("id", upload->upload_id);

    std::u16string value_w;
    if (!upload->upload_time.is_null()) {
      value_w = base::TimeFormatFriendlyDateAndTime(upload->upload_time);
    }
    upload_value.Set("upload_time", value_w);

    std::string value;
    if (!upload->local_id.empty()) {
      value = text_log_dir_.AppendASCII(upload->local_id)
                  .AddExtension(FILE_PATH_LITERAL(".gz"))
                  .AsUTF8Unsafe();
    }
    upload_value.Set("local_file", value);

    // In october 2015, capture time was added to the log list, previously the
    // local ID was used as capture time. The local ID has however changed so
    // that it might not be a time. We fall back on the local ID if it traslates
    // to a time within reasonable bounds, otherwise we fall back on the upload
    // time.
    // TODO(grunell): Use |capture_time| only.
    if (!upload->capture_time.is_null()) {
      value_w = base::TimeFormatFriendlyDateAndTime(upload->capture_time);
    } else {
      // Fall back on local ID as time. We need to check that it's within
      // resonable bounds, since the ID may not represent time. Check between
      // 2012 when the feature was introduced and now.
      double seconds_since_epoch;
      if (base::StringToDouble(upload->local_id, &seconds_since_epoch)) {
        base::Time capture_time =
            base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
        static constexpr base::Time::Exploded kLowerLimit = {
            .year = 2012, .month = 1, .day_of_month = 1};
        base::Time out_time;
        bool conversion_success =
            base::Time::FromUTCExploded(kLowerLimit, &out_time);
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
      value_w = std::u16string(u"(unknown time)");
    upload_value.Set("capture_time", value_w);

    result.Append(std::move(upload_value));
  }
  return result;
}

base::Value::List WebRtcLogsDOMHandler::UpdateUIWithEventLogs() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::List result;
  for (const auto& log : event_logs_) {
    result.Append(EventLogUploadInfoToValue(log));
  }
  return result;
}

base::Value WebRtcLogsDOMHandler::EventLogUploadInfoToValue(
    const UploadList::UploadInfo& info) const {
  switch (info.state) {
    case UploadList::UploadInfo::State::Pending:
      // TODO(crbug.com/40545136): Display actively-written logs differently
      // than fully captured pending logs.
      return info.upload_time.is_null() ? FromPendingLog(info)
                                        : FromActivelyUploadedLog(info);
    case UploadList::UploadInfo::State::NotUploaded:
      return info.upload_time.is_null() ? FromNotUploadedLog(info)
                                        : FromUploadUnsuccessfulLog(info);
    case UploadList::UploadInfo::State::Uploaded:
      return FromUploadSuccessfulLog(info);
    case UploadList::UploadInfo::State::Pending_UserRequested:
      NOTREACHED_IN_MIGRATION();
  }

  LOG(ERROR) << "Unrecognized state (" << static_cast<int>(info.state) << ").";
  return base::Value();
}

base::Value WebRtcLogsDOMHandler::FromPendingLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::Pending);
  DCHECK(info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return base::Value();
  }

  base::Value::Dict log;
  log.Set("state", "pending");
  log.Set("capture_time",
          base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log.Set("local_file",
          event_log_dir_.AppendASCII(info.local_id).AsUTF8Unsafe());
  return base::Value(std::move(log));
}

base::Value WebRtcLogsDOMHandler::FromActivelyUploadedLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::Pending);
  DCHECK(!info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return base::Value();
  }

  base::Value::Dict log;
  log.Set("state", "actively_uploaded");
  log.Set("capture_time",
          base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log.Set("local_file",
          event_log_dir_.AppendASCII(info.local_id).AsUTF8Unsafe());
  return base::Value(std::move(log));
}

base::Value WebRtcLogsDOMHandler::FromNotUploadedLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::NotUploaded);
  DCHECK(info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return base::Value();
  }

  base::Value::Dict log;
  log.Set("state", "not_uploaded");
  log.Set("capture_time",
          base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log.Set("local_id", info.local_id);
  return base::Value(std::move(log));
}

base::Value WebRtcLogsDOMHandler::FromUploadUnsuccessfulLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::NotUploaded);
  DCHECK(!info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return base::Value();
  }

  if (!info.upload_id.empty()) {
    LOG(ERROR) << "Unexpected upload ID.";
    return base::Value();
  }

  base::Value::Dict log;
  log.Set("state", "upload_unsuccessful");
  log.Set("capture_time",
          base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log.Set("local_id", info.local_id);
  log.Set("upload_time", base::TimeFormatFriendlyDateAndTime(info.upload_time));
  return base::Value(std::move(log));
}

base::Value WebRtcLogsDOMHandler::FromUploadSuccessfulLog(
    const UploadList::UploadInfo& info) const {
  DCHECK_EQ(info.state, UploadList::UploadInfo::State::Uploaded);
  DCHECK(!info.upload_time.is_null());

  if (!SanityCheckOnUploadInfo(info)) {
    return base::Value();
  }

  if (info.upload_id.empty()) {
    LOG(ERROR) << "Unknown upload ID.";
    return base::Value();
  }

  base::Value::Dict log;
  log.Set("state", "upload_successful");
  log.Set("capture_time",
          base::TimeFormatFriendlyDateAndTime(info.capture_time));
  log.Set("local_id", info.local_id);
  log.Set("upload_id", info.upload_id);
  log.Set("upload_time", base::TimeFormatFriendlyDateAndTime(info.upload_time));
  return base::Value(std::move(log));
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
  CreateAndAddWebRtcLogsUIHTMLSource(profile);
}
