// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestion.h"

#include <limits>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "components/ntp_snippets/time_serialization.h"

namespace {

// dict.Get() specialization for base::Time values
bool GetTimeValue(const base::DictionaryValue& dict,
                  const std::string& key,
                  base::Time* time) {
  std::string time_value;
  return dict.GetString(key, &time_value) &&
         base::Time::FromString(time_value.c_str(), time);
}

// dict.Get() specialization for GURL values
bool GetURLValue(const base::DictionaryValue& dict,
                 const std::string& key,
                 GURL* url) {
  std::string spec;
  if (!dict.GetString(key, &spec)) {
    return false;
  }
  *url = GURL(spec);
  return url->is_valid();
}

}  // namespace

namespace ntp_snippets {

const int kArticlesRemoteId = 1;
static_assert(
    static_cast<int>(KnownCategories::ARTICLES) -
            static_cast<int>(KnownCategories::REMOTE_CATEGORIES_OFFSET) ==
        kArticlesRemoteId,
    "kArticlesRemoteId has a wrong value?!");

RemoteSuggestion::RemoteSuggestion(const std::vector<std::string>& ids,
                                   int remote_category_id)
    : ids_(ids),
      score_(0),
      is_dismissed_(false),
      remote_category_id_(remote_category_id),
      rank_(std::numeric_limits<int>::max()),
      should_notify_(false),
      content_type_(ContentType::UNKNOWN) {}

RemoteSuggestion::~RemoteSuggestion() = default;

// static
std::unique_ptr<RemoteSuggestion>
RemoteSuggestion::CreateFromContentSuggestionsDictionary(
    const base::DictionaryValue& dict,
    int remote_category_id,
    const base::Time& fetch_date) {
  const base::ListValue* ids;
  if (!dict.GetList("ids", &ids)) {
    return nullptr;
  }
  std::vector<std::string> parsed_ids;
  for (const auto& value : *ids) {
    std::string id;
    if (!value.GetAsString(&id)) {
      return nullptr;
    }
    parsed_ids.push_back(id);
  }

  if (parsed_ids.empty()) {
    return nullptr;
  }
  auto snippet = MakeUnique(parsed_ids, remote_category_id);
  snippet->fetch_date_ = fetch_date;

  if (!(dict.GetString("title", &snippet->title_) &&
        GetTimeValue(dict, "creationTime", &snippet->publish_date_) &&
        GetTimeValue(dict, "expirationTime", &snippet->expiry_date_) &&
        dict.GetString("attribution", &snippet->publisher_name_) &&
        GetURLValue(dict, "fullPageUrl", &snippet->url_))) {
    return nullptr;
  }

  // Optional fields.
  dict.GetString("snippet", &snippet->snippet_);
  GetURLValue(dict, "imageUrl", &snippet->salient_image_url_);
  GetURLValue(dict, "ampUrl", &snippet->amp_url_);

  // TODO(sfiera): also favicon URL.

  const base::Value* image_dominant_color_value =
      dict.FindKey("imageDominantColor");
  if (image_dominant_color_value) {
    // The field is defined as fixed32 in the proto (effectively 32 bits
    // unsigned int), however, JSON does not support unsigned types. As a result
    // the value is parsed as int if it fits and as double otherwise. Double can
    // hold 32 bits precisely.
    uint32_t image_dominant_color;
    if (image_dominant_color_value->is_int()) {
      image_dominant_color = image_dominant_color_value->GetInt();
    } else if (image_dominant_color_value->is_double()) {
      image_dominant_color =
          static_cast<uint32_t>(image_dominant_color_value->GetDouble());
    }
    snippet->image_dominant_color_ = image_dominant_color;
  }

  double score;
  if (dict.GetDouble("score", &score)) {
    snippet->score_ = score;
  }

  const base::DictionaryValue* notification_info = nullptr;
  if (dict.GetDictionary("notificationInfo", &notification_info)) {
    if (notification_info->GetBoolean("shouldNotify",
                                      &snippet->should_notify_) &&
        snippet->should_notify_) {
      if (!GetTimeValue(*notification_info, "deadline",
                        &snippet->notification_deadline_)) {
        snippet->notification_deadline_ = base::Time::Max();
      }
    }
  }

  // In the JSON dictionary contentType is an optional field. The field
  // content_type_ of the class |RemoteSuggestion| is by default initialized to
  // ContentType::UNKNOWN.
  std::string content_type;
  if (dict.GetString("contentType", &content_type)) {
    if (content_type == "VIDEO") {
      snippet->content_type_ = ContentType::VIDEO;
    } else {
      // The supported values are: VIDEO, UNKNOWN. Therefore if the field is
      // present the value has to be "UNKNOWN" here.
      DCHECK_EQ(content_type, "UNKNOWN");
      snippet->content_type_ = ContentType::UNKNOWN;
    }
  }

  return snippet;
}

// static
std::unique_ptr<RemoteSuggestion> RemoteSuggestion::CreateFromProto(
    const SnippetProto& proto) {
  // Need at least the id.
  if (proto.ids_size() == 0 || proto.ids(0).empty()) {
    return nullptr;
  }

  int remote_category_id = proto.has_remote_category_id()
                               ? proto.remote_category_id()
                               : kArticlesRemoteId;

  std::vector<std::string> ids(proto.ids().begin(), proto.ids().end());

  auto snippet = MakeUnique(ids, remote_category_id);

  snippet->title_ = proto.title();
  snippet->snippet_ = proto.snippet();

  snippet->salient_image_url_ = GURL(proto.salient_image_url());
  if (proto.has_image_dominant_color()) {
    snippet->image_dominant_color_ = proto.image_dominant_color();
  }

  snippet->publish_date_ = DeserializeTime(proto.publish_date());
  snippet->expiry_date_ = DeserializeTime(proto.expiry_date());
  snippet->score_ = proto.score();
  snippet->is_dismissed_ = proto.dismissed();

  if (!proto.has_source()) {
    DLOG(WARNING) << "No source found for article " << snippet->id();
    return nullptr;
  }
  GURL url(proto.source().url());
  if (!url.is_valid()) {
    // We must at least have a valid source URL.
    DLOG(WARNING) << "Invalid article url " << proto.source().url();
    return nullptr;
  }
  GURL amp_url;
  if (proto.source().has_amp_url()) {
    amp_url = GURL(proto.source().amp_url());
    DLOG_IF(WARNING, !amp_url.is_valid())
        << "Invalid AMP URL " << proto.source().amp_url();
  }
  snippet->url_ = url;
  snippet->publisher_name_ = proto.source().publisher_name();
  snippet->amp_url_ = amp_url;

  if (proto.has_fetch_date()) {
    snippet->fetch_date_ = DeserializeTime(proto.fetch_date());
  }

  if (proto.content_type() == SnippetProto_ContentType_VIDEO) {
    snippet->content_type_ = ContentType::VIDEO;
  }

  snippet->rank_ =
      proto.has_rank() ? proto.rank() : std::numeric_limits<int>::max();

  return snippet;
}

SnippetProto RemoteSuggestion::ToProto() const {
  SnippetProto result;
  for (const std::string& id : ids_) {
    result.add_ids(id);
  }
  if (!title_.empty()) {
    result.set_title(title_);
  }
  if (!snippet_.empty()) {
    result.set_snippet(snippet_);
  }
  if (salient_image_url_.is_valid()) {
    result.set_salient_image_url(salient_image_url_.spec());
  }
  if (image_dominant_color_.has_value()) {
    result.set_image_dominant_color(*image_dominant_color_);
  }
  if (!publish_date_.is_null()) {
    result.set_publish_date(SerializeTime(publish_date_));
  }
  if (!expiry_date_.is_null()) {
    result.set_expiry_date(SerializeTime(expiry_date_));
  }
  result.set_score(score_);
  result.set_dismissed(is_dismissed_);
  result.set_remote_category_id(remote_category_id_);

  SnippetSourceProto* source_proto = result.mutable_source();
  source_proto->set_url(url_.spec());
  if (!publisher_name_.empty()) {
    source_proto->set_publisher_name(publisher_name_);
  }
  if (amp_url_.is_valid()) {
    source_proto->set_amp_url(amp_url_.spec());
  }

  if (!fetch_date_.is_null()) {
    result.set_fetch_date(SerializeTime(fetch_date_));
  }

  if (content_type_ == ContentType::VIDEO) {
    result.set_content_type(SnippetProto_ContentType_VIDEO);
  }

  result.set_rank(rank_);

  return result;
}

ContentSuggestion RemoteSuggestion::ToContentSuggestion(
    Category category) const {
  GURL url = url_;
  bool use_amp = !amp_url_.is_empty();
  if (use_amp) {
    url = amp_url_;
  }
  ContentSuggestion suggestion(category, id(), url);
  // Set url for fetching favicons if it differs from the main url (domains of
  // AMP URLs sometimes failed to provide favicons).
  if (use_amp) {
    suggestion.set_url_with_favicon(url_);
  }
  suggestion.set_title(base::UTF8ToUTF16(title_));
  suggestion.set_snippet_text(base::UTF8ToUTF16(snippet_));
  suggestion.set_publish_date(publish_date_);
  suggestion.set_publisher_name(base::UTF8ToUTF16(publisher_name_));
  suggestion.set_score(score_);
  suggestion.set_salient_image_url(salient_image_url_);

  if (should_notify_) {
    NotificationExtra extra;
    extra.deadline = notification_deadline_;
    suggestion.set_notification_extra(
        std::make_unique<NotificationExtra>(extra));
  }
  suggestion.set_fetch_date(fetch_date_);
  if (content_type_ == ContentType::VIDEO) {
    suggestion.set_is_video_suggestion(true);
  }
  suggestion.set_optional_image_dominant_color(image_dominant_color_);
  return suggestion;
}

// static
std::unique_ptr<RemoteSuggestion> RemoteSuggestion::MakeUnique(
    const std::vector<std::string>& ids,
    int remote_category_id) {
  return base::WrapUnique(new RemoteSuggestion(ids, remote_category_id));
}

}  // namespace ntp_snippets
