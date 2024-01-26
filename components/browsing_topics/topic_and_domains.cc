// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/topic_and_domains.h"

#include "base/check_op.h"
#include "base/json/values_util.h"
#include "url/gurl.h"

namespace browsing_topics {

namespace {

const char kTopicNameKey[] = "topic";
const char kHashedDomainsNameKey[] = "hashed_domains";

}  // namespace

TopicAndDomains::TopicAndDomains() = default;

TopicAndDomains::TopicAndDomains(Topic topic,
                                 std::set<HashedDomain> hashed_domains)
    : topic_(std::move(topic)), hashed_domains_(std::move(hashed_domains)) {}

TopicAndDomains::TopicAndDomains(TopicAndDomains&& other) = default;

TopicAndDomains& TopicAndDomains::operator=(TopicAndDomains&& other) = default;

TopicAndDomains::~TopicAndDomains() = default;

// static
TopicAndDomains TopicAndDomains::FromDictValue(
    const base::Value::Dict& dict_value) {
  Topic topic(0);
  std::optional<int> topic_value = dict_value.FindInt(kTopicNameKey);
  if (topic_value)
    topic = Topic(*topic_value);

  std::set<HashedDomain> hashed_domains;
  const base::Value::List* hashed_domains_value =
      dict_value.FindList(kHashedDomainsNameKey);
  if (hashed_domains_value) {
    for (const base::Value& hashed_domain_value : *hashed_domains_value) {
      std::optional<int64_t> hashed_domain_int64_value =
          base::ValueToInt64(hashed_domain_value);
      if (!hashed_domain_int64_value)
        return TopicAndDomains();

      hashed_domains.insert(HashedDomain(hashed_domain_int64_value.value()));
    }
  }

  return TopicAndDomains(topic, std::move(hashed_domains));
}

base::Value::Dict TopicAndDomains::ToDictValue() const {
  base::Value::List hashed_domains_list;
  for (const HashedDomain& hashed_domain : hashed_domains_) {
    hashed_domains_list.Append(base::Int64ToValue(hashed_domain.value()));
  }

  base::Value::Dict result_dict;
  result_dict.Set(kTopicNameKey, topic_.value());
  result_dict.Set(kHashedDomainsNameKey, std::move(hashed_domains_list));
  return result_dict;
}

void TopicAndDomains::ClearDomain(const HashedDomain& domain) {
  hashed_domains_.erase(domain);
}

}  // namespace browsing_topics
