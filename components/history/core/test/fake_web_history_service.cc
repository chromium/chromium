// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/fake_web_history_service.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/sync/protocol/history_status.pb.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace history {

// FakeRequest -----------------------------------------------------------------

namespace {

// TODO(msramek): Find a way to keep these URLs in sync with what is used
// in WebHistoryService.

const char kLookupUrl[] = "https://history.google.com/history/api/lookup";

const char kDeleteUrl[] = "https://history.google.com/history/api/delete";

const char kChromeClient[] = "chrome";

const char kWebAndAppClient[] = "web_app";

const char kSyncServerHost[] = "clients4.google.com";

}  // namespace

class FakeWebHistoryService::FakeRequest : public WebHistoryService::Request {
 public:
  FakeRequest(FakeWebHistoryService* service,
              const GURL& url,
              bool emulate_success,
              int emulate_response_code,
              WebHistoryService::CompletionCallback callback,
              base::Time begin,
              base::Time end,
              int max_count);

  // WebHistoryService::Request implementation.
  bool IsPending() override;
  int GetResponseCode() override;
  const std::string& GetResponseBody() override;
  void SetPostData(const std::string& post_data) override;
  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override;
  void SetUserAgent(const std::string& user_agent) override;
  void Start() override;

 private:
  FakeWebHistoryService* service_;
  GURL url_;
  bool emulate_success_;
  int emulate_response_code_;
  WebHistoryService::CompletionCallback callback_;
  base::Time begin_;
  base::Time end_;
  int max_count_;
  bool is_pending_;
  std::string response_body_;

  DISALLOW_COPY_AND_ASSIGN(FakeRequest);
};

FakeWebHistoryService::FakeRequest::FakeRequest(
    FakeWebHistoryService* service,
    const GURL& url,
    bool emulate_success,
    int emulate_response_code,
    WebHistoryService::CompletionCallback callback,
    base::Time begin,
    base::Time end,
    int max_count)
    : service_(service),
      url_(url),
      emulate_success_(emulate_success),
      emulate_response_code_(emulate_response_code),
      callback_(std::move(callback)),
      begin_(begin),
      end_(end),
      max_count_(max_count),
      is_pending_(false) {}

bool FakeWebHistoryService::FakeRequest::IsPending() {
  return is_pending_;
}

int FakeWebHistoryService::FakeRequest::GetResponseCode() {
  return emulate_response_code_;
}

const std::string& FakeWebHistoryService::FakeRequest::GetResponseBody() {
  std::string client;
  net::GetValueForKeyInQuery(url_, "client", &client);

  GURL::Replacements remove_query;
  remove_query.ClearQuery();
  GURL base_url = url_.ReplaceComponents(remove_query);

  if (base_url == kLookupUrl && client == kChromeClient) {
    // History query.
    response_body_ = "{ \"event\": [";
    bool more_results_left;
    auto visits = service_->GetVisitsBetween(begin_, end_, max_count_,
                                             &more_results_left);
    std::vector<std::string> results;
    for (const FakeWebHistoryService::Visit& visit : visits) {
      std::string unix_time = std::to_string(
          (visit.timestamp - base::Time::UnixEpoch()).InMicroseconds());
      results.push_back(base::StringPrintf(
          "{\"result\":[{\"id\":[{\"timestamp_usec\":\"%s\"}"
          "],\"url\":\"%s\",\"favicon_url\":\"%s\"}]}",
          unix_time.c_str(), visit.url.c_str(), visit.icon_url.c_str()));
    }
    response_body_ += base::JoinString(results, ",");
    response_body_ +=
        base::StringPrintf("], \"continuation_token\":\"%s\" }",
                           (more_results_left ? "more_results_left" : ""));

  } else if (base_url == kDeleteUrl && client == kChromeClient) {
    // Deletion query.
    response_body_ = "{ \"just needs to be\" : \"a valid JSON.\" }";

  } else if (base_url == kLookupUrl && client == kWebAndAppClient) {
    // Web and app activity query.
    response_body_ = base::StringPrintf(
        "{ \"history_recording_enabled\": %s }",
        service_->IsWebAndAppActivityEnabled() ? "true" : "false");

  } else if (url_.host() == kSyncServerHost) {
    // Other forms of browsing history query.
    std::unique_ptr<sync_pb::HistoryStatusResponse> history_status(
        new sync_pb::HistoryStatusResponse());
    history_status->set_has_derived_data(
        service_->AreOtherFormsOfBrowsingHistoryPresent());
    history_status->SerializeToString(&response_body_);
  }

  return response_body_;
}

void FakeWebHistoryService::FakeRequest::SetPostData(
    const std::string& post_data) {
  // Unused.
}

void FakeWebHistoryService::FakeRequest::SetPostDataAndType(
    const std::string& post_data,
    const std::string& mime_type) {
  // Unused.
}

void FakeWebHistoryService::FakeRequest::SetUserAgent(
    const std::string& user_agent) {
  // Unused.
}

void FakeWebHistoryService::FakeRequest::Start() {
  is_pending_ = true;
  std::move(callback_).Run(this, emulate_success_);
}

// FakeWebHistoryService -------------------------------------------------------

FakeWebHistoryService::FakeWebHistoryService()
    // NOTE: Simply pass null object for IdentityManager. WebHistoryService's
    // only usage of this object is to fetch access tokens via RequestImpl, and
    // FakeWebHistoryService deliberately replaces this flow with
    // FakeWebHistoryService::FakeRequest.
    : history::WebHistoryService(nullptr, nullptr),
      emulate_success_(true),
      emulate_response_code_(net::HTTP_OK),
      web_and_app_activity_enabled_(false),
      other_forms_of_browsing_history_present_(false) {}

FakeWebHistoryService::~FakeWebHistoryService() {
}

void FakeWebHistoryService::SetupFakeResponse(
    bool emulate_success, int emulate_response_code) {
  emulate_success_ = emulate_success;
  emulate_response_code_ = emulate_response_code;
}

void FakeWebHistoryService::AddSyncedVisit(const std::string& url,
                                           base::Time timestamp,
                                           const std::string& icon_url) {
  visits_.emplace_back(Visit(url, timestamp, icon_url));
}

void FakeWebHistoryService::ClearSyncedVisits() {
  visits_.clear();
}

std::vector<FakeWebHistoryService::Visit>
FakeWebHistoryService::GetVisitsBetween(base::Time begin,
                                        base::Time end,
                                        size_t count,
                                        bool* more_results_left) {
  // Make sure that |visits_| is sorted in reverse chronological order before we
  // return anything. This means that the most recent results are returned
  // first.
  std::sort(visits_.begin(), visits_.end(),
            [](const Visit& lhs, const Visit rhs) -> bool {
              return lhs.timestamp > rhs.timestamp;
            });
  *more_results_left = false;
  std::vector<Visit> result;
  for (const Visit& visit : visits_) {
    // |begin| is inclusive, |end| is exclusive.
    if (visit.timestamp >= begin && visit.timestamp < end) {
      // We found another valid result, but cannot return it because we've
      // reached max count.
      if (count > 0 && result.size() >= count) {
        *more_results_left = true;
        break;
      }

      result.push_back(visit);
    }
  }
  return result;
}

base::Time FakeWebHistoryService::GetTimeForKeyInQuery(
    const GURL& url, const std::string& key) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, key, &value))
    return base::Time();

  int64_t us;
  if (!base::StringToInt64(value, &us))
     return base::Time();
  return base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(us);
}

FakeWebHistoryService::Request* FakeWebHistoryService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Find the time range endpoints in the URL.
  base::Time begin = GetTimeForKeyInQuery(url, "min");
  base::Time end = GetTimeForKeyInQuery(url, "max");

  if (end.is_null())
    end = base::Time::Max();

  int max_count = 0;
  std::string max_count_str;
  if (net::GetValueForKeyInQuery(url, "num", &max_count_str))
    base::StringToInt(max_count_str, &max_count);

  return new FakeRequest(this, url, emulate_success_, emulate_response_code_,
                         std::move(callback), begin, end, max_count);
}

bool FakeWebHistoryService::IsWebAndAppActivityEnabled() {
  return web_and_app_activity_enabled_;
}

void FakeWebHistoryService::SetWebAndAppActivityEnabled(bool enabled) {
  web_and_app_activity_enabled_ = enabled;
}

bool FakeWebHistoryService::AreOtherFormsOfBrowsingHistoryPresent() {
  return other_forms_of_browsing_history_present_;
}

void FakeWebHistoryService::SetOtherFormsOfBrowsingHistoryPresent(
    bool present) {
  other_forms_of_browsing_history_present_ = present;
}

FakeWebHistoryService::Visit::Visit(const std::string& url,
                                    base::Time timestamp,
                                    const std::string& icon_url)
    : url(url), timestamp(timestamp), icon_url(icon_url) {}

}  // namespace history
