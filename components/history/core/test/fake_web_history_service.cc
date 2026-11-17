// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/fake_web_history_service.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "components/sync/protocol/history_status.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace history {

// FakeRequest -----------------------------------------------------------------

namespace {

// TODO(msramek): Find a way to keep these URLs in sync with what is used
// in WebHistoryService.

const char kLookupUrl[] =
    "https://footprints-pa.googleapis.com/v1/read_chrome_history";

const char kDeleteUrl[] =
    "https://footprints-pa.googleapis.com/v1/delete_chrome_history";

const char kQueryWebAndAppActivityUrl[] =
    "https://footprints-pa.googleapis.com/v1/get_facs";

const char kSyncServerHost[] = "clients4.google.com";

}  // namespace

class FakeWebHistoryService::FakeRequest : public WebHistoryService::Request {
 public:
  FakeRequest(FakeWebHistoryService* service,
              const GURL& url,
              bool emulate_success,
              int emulate_response_code,
              WebHistoryService::CompletionCallback callback);

  FakeRequest(const FakeRequest&) = delete;
  FakeRequest& operator=(const FakeRequest&) = delete;

  // WebHistoryService::Request implementation.
  bool IsPending() const override;
  int GetResponseCode() const override;
  const std::string& GetResponseBody() const override;
  void SetPostData(const std::string& post_data) override;
  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override;
  void SetUserAgent(const std::string& user_agent) override;
  void Start() override;

 private:
  raw_ptr<FakeWebHistoryService> service_;
  GURL url_;
  bool emulate_success_;
  int emulate_response_code_;
  WebHistoryService::CompletionCallback callback_;
  bool is_pending_ = false;
  std::string post_data_;
  // Needed only because `GetResponseBody()` returns a reference.
  mutable std::string response_body_;
};

FakeWebHistoryService::FakeRequest::FakeRequest(
    FakeWebHistoryService* service,
    const GURL& url,
    bool emulate_success,
    int emulate_response_code,
    WebHistoryService::CompletionCallback callback)
    : service_(service),
      url_(url),
      emulate_success_(emulate_success),
      emulate_response_code_(emulate_response_code),
      callback_(std::move(callback)) {}

bool FakeWebHistoryService::FakeRequest::IsPending() const {
  return is_pending_;
}

int FakeWebHistoryService::FakeRequest::GetResponseCode() const {
  return emulate_response_code_;
}

const std::string& FakeWebHistoryService::FakeRequest::GetResponseBody() const {
  GURL::Replacements remove_query;
  remove_query.ClearQuery();
  GURL base_url = url_.ReplaceComponents(remove_query);

  if (base_url == kLookupUrl) {
    // History query.
    // First parse the request parameters out of the post data.
    base::Time begin;
    base::Time end;
    int max_count = 0;
    std::vector<std::string> client_ids;
    std::string hostname_suffix;
    std::optional<base::DictValue> request_data =
        base::JSONReader::ReadDict(post_data_, base::JSON_PARSE_RFC);
    if (request_data) {
      const base::ListValue* lookup_list = request_data->FindList("lookup");
      if (lookup_list && !lookup_list->empty()) {
        const base::Value& lookup = *lookup_list->begin();
        if (lookup.is_dict()) {
          const base::DictValue& lookup_dict = lookup.GetDict();

          max_count = lookup_dict.FindInt("max_num_results").value_or(0);

          const std::string* min_time_str =
              lookup_dict.FindString("min_timestamp_usec");
          int64_t min_time_us = 0;
          if (min_time_str &&
              base::StringToInt64(*min_time_str, &min_time_us)) {
            begin = base::Time::UnixEpoch() + base::Microseconds(min_time_us);
          }

          const std::string* max_time_str =
              lookup_dict.FindString("max_timestamp_usec");
          int64_t max_time_us = 0;
          if (max_time_str &&
              base::StringToInt64(*max_time_str, &max_time_us)) {
            end = base::Time::UnixEpoch() + base::Microseconds(max_time_us);
          }

          const base::ListValue* client_id_list =
              lookup_dict.FindList("client_id");
          if (client_id_list) {
            for (const base::Value& id_val : *client_id_list) {
              if (id_val.is_string()) {
                client_ids.push_back(id_val.GetString());
              }
            }
          }

          const base::ListValue* hostname_suffix_list =
              lookup_dict.FindList("hostname_suffix");
          if (hostname_suffix_list && !hostname_suffix_list->empty() &&
              hostname_suffix_list->front().is_string()) {
            hostname_suffix = hostname_suffix_list->front().GetString();
          }
        }
      }
    }
    if (end.is_null()) {
      end = base::Time::Max();
    }

    bool more_results_left = false;
    auto visits =
        service_->GetVisitsBetween(begin, end, max_count, client_ids,
                                   hostname_suffix, &more_results_left);

    std::vector<std::string> results;
    for (const FakeWebHistoryService::Visit& visit : visits) {
      std::string unix_time = base::NumberToString(
          (visit.timestamp - base::Time::UnixEpoch()).InMicroseconds());
      results.push_back(base::StringPrintf(
          "{\"timestamp\":\"%s\",\"url\":\"%s\",\"faviconUrl\":\"%s\","
          "\"clientId\":\"%s\"}",
          unix_time.c_str(), visit.url.c_str(), visit.icon_url.c_str(),
          visit.client_id.c_str()));
    }
    response_body_ = "{ \"lookup\": [{ \"chromeHistory\": [";
    response_body_ += base::JoinString(results, ",");
    response_body_ += base::StringPrintf(
        "], \"hasMoreResults\":%s }]}", (more_results_left ? "true" : "false"));

  } else if (base_url == kDeleteUrl) {
    // Deletion query.
    response_body_ = "{ \"just needs to be\" : \"a valid JSON.\" }";

  } else if (base_url == kQueryWebAndAppActivityUrl) {
    // Web and app activity query.
    response_body_ = base::StringPrintf(
        "{ \"facsSetting\": [ {\"dataRecordingEnabled\": %s} ]}",
        base::ToString(service_->IsWebAndAppActivityEnabled()));

  } else if (url_.GetHost() == kSyncServerHost) {
    // Other forms of browsing history query.
    auto history_status = std::make_unique<sync_pb::HistoryStatusResponse>();
    history_status->set_has_derived_data(
        service_->AreOtherFormsOfBrowsingHistoryPresent());
    history_status->SerializeToString(&response_body_);
  }

  return response_body_;
}

void FakeWebHistoryService::FakeRequest::SetPostData(
    const std::string& post_data) {
  SetPostDataAndType(post_data, "text/plain");
}

void FakeWebHistoryService::FakeRequest::SetPostDataAndType(
    const std::string& post_data,
    const std::string& mime_type) {
  post_data_ = post_data;
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
    : history::WebHistoryService(nullptr, nullptr, nullptr),
      emulate_success_(true),
      emulate_response_code_(net::HTTP_OK),
      web_and_app_activity_enabled_(false),
      other_forms_of_browsing_history_present_(false) {
  // FakeWebHistoryService acts as if history sync is always enabled.
  is_enabled_ = true;
}

FakeWebHistoryService::~FakeWebHistoryService() = default;

void FakeWebHistoryService::SetupFakeResponse(bool emulate_success,
                                              int emulate_response_code) {
  emulate_success_ = emulate_success;
  emulate_response_code_ = emulate_response_code;
}

void FakeWebHistoryService::AddSyncedVisit(const std::string& url,
                                           base::Time timestamp,
                                           const std::string& icon_url,
                                           const std::string& client_id) {
  visits_.emplace_back(Visit(url, timestamp, icon_url, client_id));
}

void FakeWebHistoryService::ClearSyncedVisits() {
  visits_.clear();
}

std::vector<FakeWebHistoryService::Visit>
FakeWebHistoryService::GetVisitsBetween(
    base::Time begin,
    base::Time end,
    size_t count,
    const std::vector<std::string>& client_ids,
    const std::string& hostname_suffix,
    bool* more_results_left) {
  // Make sure that `visits_` is sorted in reverse chronological order before we
  // return anything. This means that the most recent results are returned
  // first.
  std::sort(visits_.begin(), visits_.end(),
            [](const Visit& lhs, const Visit& rhs) -> bool {
              return lhs.timestamp > rhs.timestamp;
            });
  *more_results_left = false;
  std::string lower_suffix = base::ToLowerASCII(hostname_suffix);
  std::vector<Visit> result;
  for (const Visit& visit : visits_) {
    // `begin` is inclusive, `end` is exclusive.
    if (visit.timestamp >= begin && visit.timestamp < end) {
      if (!client_ids.empty() &&
          !std::ranges::contains(client_ids, visit.client_id)) {
        continue;
      }

      if (!lower_suffix.empty()) {
        GURL url(visit.url);
        if (!url.DomainIs(lower_suffix)) {
          continue;
        }
      }

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

std::unique_ptr<WebHistoryService::Request>
FakeWebHistoryService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  return std::make_unique<FakeRequest>(
      this, url, emulate_success_, emulate_response_code_, std::move(callback));
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
                                    const std::string& icon_url,
                                    const std::string& client_id)
    : url(url),
      timestamp(timestamp),
      icon_url(icon_url),
      client_id(client_id.empty() ? kDefaultClientId : client_id) {}

FakeWebHistoryService::Visit::Visit(const Visit&) = default;
FakeWebHistoryService::Visit& FakeWebHistoryService::Visit::operator=(
    const Visit&) = default;
FakeWebHistoryService::Visit::Visit(Visit&&) noexcept = default;
FakeWebHistoryService::Visit& FakeWebHistoryService::Visit::operator=(
    Visit&&) noexcept = default;
FakeWebHistoryService::Visit::~Visit() = default;

}  // namespace history
