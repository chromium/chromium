// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_remote_model_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/history_clusters/core/memories_features.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace history_clusters {

namespace {

const size_t kMaxExpectedResponseSize = 1024 * 1024;

// Helpers to translate from |MemoriesVisit|s to |base::Value|; and from
// |base::Value| to |mojom::MemoryPtr|s.

base::Value VisitToValue(const MemoriesVisit& visit) {
  // bas::Value does not support long ints (i.e. int64_t), so they're cast to
  // strings. Casting them to doubles would convert large values to scientific
  // notation which would be problematic (i.e. impossible to distinguish x and
  // x+1).
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("visitId",
              base::Value(base::NumberToString(visit.visit_row.visit_id)));
  dict.SetKey("url", base::Value(visit.url_row.url().spec()));
  dict.SetKey("origin", base::Value(visit.url_row.url().GetOrigin().spec()));
  dict.SetKey("foregroundTimeSecs", base::Value(0));
  dict.SetKey("navigationTimeMs",
              base::Value(base::NumberToString(
                  visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch()
                      .InMilliseconds())));
  dict.SetKey("siteEngagementScore", base::Value(0));
  dict.SetKey("pageEndReason",
              base::Value(visit.context_signals.page_end_reason));
  dict.SetKey("pageTransition",
              base::Value(static_cast<int>(visit.visit_row.transition)));
  dict.SetKey("isFromGoogleSearch", base::Value(false));
  // TODO(manukh) fill out:
  //  |foregroundTimeSecs|
  //  |siteEngagementScore|
  //  |isFromGoogleSearch|
  return dict;
}

base::Value VisitsToValue(const std::vector<MemoriesVisit>& visits) {
  base::Value visits_list(base::Value::Type::LIST);
  for (const auto& visit : visits)
    visits_list.Append(VisitToValue(visit));

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("visits", std::move(visits_list));
  return dict;
}

// A helper method to get the values of the |base::ListValue| at |value[key]|.
template <typename T>
std::vector<T> FindListKeyAndCast(
    const base::Value& value,
    std::string key,
    base::RepeatingCallback<T(const base::Value&)> cast_callback) {
  const base::Value* list_ptr = value.FindListKey(key);
  if (!list_ptr)
    return {};

  base::Value::ConstListView list = list_ptr->GetList();
  std::vector<T> casted(list.size());

  base::ranges::transform(list, casted.begin(), [&](const base::Value& value) {
    return cast_callback.Run(value);
  });
  return casted;
}

mojom::VisitPtr ValueToVisit(const std::vector<MemoriesVisit>& visits,
                             const base::Value& visit_id_value) {
  auto visit = mojom::Visit::New();
  if (!visit_id_value.is_string() ||
      !base::StringToInt64(visit_id_value.GetString(), &visit->id))
    visit->id = 0;
  const auto memory_visit_it = base::ranges::find(
      visits, visit->id,
      [](const auto& visit) { return visit.visit_row.visit_id; });
  if (memory_visit_it != visits.end()) {
    visit->url = memory_visit_it->url_row.url();
    visit->time = memory_visit_it->visit_row.visit_time;
    visit->page_title = base::UTF16ToUTF8(memory_visit_it->url_row.title());
  }

  // TODO(manukh) fill out:
  //  |thumbnail_url|
  //  |relative_date|
  //  |time_of_day|
  //  |num_duplicate_visits|
  //  |related_visits|
  //  |engagement_score|
  return visit;
}

mojom::MemoryPtr ValueToMemory(const std::vector<MemoriesVisit>& visits,
                               const base::Value& value) {
  auto memory = mojom::Memory::New();
  memory->id = base::UnguessableToken::Create();

  memory->top_visits = FindListKeyAndCast<mojom::VisitPtr>(
      value, "visitIds", base::BindRepeating(&ValueToVisit, visits));

  memory->keywords = FindListKeyAndCast<std::u16string>(
      value, "keywords", base::BindRepeating([](const base::Value& value) {
        return value.is_string() ? base::UTF8ToUTF16(value.GetString()) : u"";
      }));

  // TODO(manukh) fill out:
  //  |id|
  //  |related_searches|
  //  |related_tab_groups|
  //  |bookmarks|
  //  |last_visit_time|
  //  |engagement_score|
  return memory;
}

Memories ValueToMemories(const std::vector<MemoriesVisit>& visits,
                         const base::Value& value) {
  return FindListKeyAndCast<mojom::MemoryPtr>(
      value, "clusters", base::BindRepeating(&ValueToMemory, visits));
}

}  // namespace

MemoriesRemoteModelHelper::MemoriesRemoteModelHelper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<void(const std::string&)> debug_logger)
    : url_loader_(nullptr),
      url_loader_factory_(url_loader_factory),
      debug_logger_(debug_logger) {}

MemoriesRemoteModelHelper::~MemoriesRemoteModelHelper() = default;

void MemoriesRemoteModelHelper::GetMemories(
    const std::vector<MemoriesVisit>& visits,
    MemoriesCallback callback) {
  const GURL endpoint(RemoteModelEndpointForDebugging());
  if (!endpoint.is_valid() || visits.empty()) {
    std::move(callback).Run({});
    return;
  }
  StopPendingRequests();

  std::string request_body;
  base::JSONWriter::Write(VisitsToValue(visits), &request_body);
  auto request = CreateRequest(endpoint);
  debug_logger_.Run(
      base::StringPrintf("MemoriesRemoteModelHelper::GetMemories request = %s",
                         request_body.c_str()));
  url_loader_ = CreateLoader(CreateRequest(endpoint), request_body);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](base::RepeatingCallback<void(const std::string&)> debug_logger,
             const std::vector<MemoriesVisit>& visits,
             MemoriesCallback callback, std::unique_ptr<std::string> response) {
            debug_logger.Run(base::StringPrintf(
                "MemoriesRemoteModelHelper::GetMemories response = %s",
                response ? response->c_str() : "nullptr"));
            if (!response) {
              std::move(callback).Run({});
              return;
            }
            data_decoder::DataDecoder::ParseJsonIsolated(
                *response,
                base::BindOnce(
                    [](const std::vector<MemoriesVisit>& visits,
                       data_decoder::DataDecoder::ValueOrError value_or_error)
                        -> Memories {
                      return value_or_error.value
                                 ? ValueToMemories(visits,
                                                   value_or_error.value.value())
                                 : Memories{};
                    },
                    visits)
                    .Then(std::move(callback)));
          },
          debug_logger_, visits, std::move(callback)),
      kMaxExpectedResponseSize);
}

void MemoriesRemoteModelHelper::StopPendingRequests() {
  // TODO(manukh): Ensure the callback for the pending request is invoked.
  url_loader_.reset();
}

// static
std::unique_ptr<network::ResourceRequest>
MemoriesRemoteModelHelper::CreateRequest(const GURL& endpoint) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(endpoint);
  request->method = "POST";
  return request;
}

// static
std::unique_ptr<network::SimpleURLLoader>
MemoriesRemoteModelHelper::CreateLoader(
    std::unique_ptr<network::ResourceRequest> request,
    const std::string& request_body) {
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("memories_remote_model_request", R"(
        semantics {
          sender: "Memories"
          description:
            "chrome://memories clusters the user's history entries into "
            "'memories' for easier browsing and searching. This request is "
            "made only if the user explicitly sets the appropriate feature and "
            "param through the command line. The param is intentionally not in "
            "chrome://flags to avoid users accidentally setting it. The "
            "request will send the user's previous navigations and searches. "
          trigger: "User must set a command line param. The request is sent "
            "for both explicit user actions (e.g. navigating, visiting "
            "chrome://memories, or typing in the omnibox) and passively (e.g. "
            " on browser startup or periodically)."
          data: "The user's navigation and search history."
          destination: OTHER
          destination_other: "With the expected param configuration, the "
            "destination will be a GOOGLE_OWNED_SERVICE. As the endpoint is "
            "set through a command line param, a user could set it to any "
            "destination."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Disabled unless the user explicitly enables the 'Memories' "
            "feature and sets the 'MemoriesRemoteModelEndpoint' param to a "
            "valid URL."
          policy_exception_justification:
            "The request is only made if the user explicitly configures it."
        })");
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  if (!request_body.empty())
    loader->AttachStringForUpload(request_body, "application/json");
  return loader;
}

}  // namespace history_clusters
