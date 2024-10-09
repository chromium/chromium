// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_host_to_urls_map.h"

#include <stddef.h>

#include <memory>

#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

// Basic functionality for the SearchHostToURLsMap tests.
class SearchHostToURLsMapTest : public testing::Test {
 public:
  SearchHostToURLsMapTest() = default;

  SearchHostToURLsMapTest(const SearchHostToURLsMapTest&) = delete;
  SearchHostToURLsMapTest& operator=(const SearchHostToURLsMapTest&) = delete;

  void SetUp() override;

 protected:
  std::unique_ptr<SearchHostToURLsMap> provider_map_;
  TemplateURLService::OwnedTemplateURLVector template_urls_;
  std::string host_;
};

void SearchHostToURLsMapTest::SetUp() {
  // Add some entries to the search host map.
  host_ = "www.unittest.com";
  TemplateURLData data;
  data.SetURL("http://" + host_ + "/path1");
  data.last_modified = base::Time() + base::Microseconds(15);
  template_urls_.push_back(std::make_unique<TemplateURL>(data));

  // The second one should be slightly newer.
  data.SetURL("http://" + host_ + "/path2");
  data.last_modified = base::Time() + base::Microseconds(25);
  template_urls_.push_back(std::make_unique<TemplateURL>(data));

  provider_map_ = std::make_unique<SearchHostToURLsMap>();
  provider_map_->Init(template_urls_, SearchTermsData());
}

TEST_F(SearchHostToURLsMapTest, Add) {
  std::string new_host = "example.com";
  TemplateURLData data;
  data.SetURL("http://" + new_host + "/");
  TemplateURL new_t_url(data);
  provider_map_->Add(&new_t_url, SearchTermsData());

  ASSERT_EQ(&new_t_url, provider_map_->GetTemplateURLForHost(new_host));
}

TEST_F(SearchHostToURLsMapTest, Remove) {
  provider_map_->Remove(template_urls_[0].get());

  const TemplateURL* found_url = provider_map_->GetTemplateURLForHost(host_);
  ASSERT_EQ(template_urls_[1].get(), found_url);

  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host_);
  ASSERT_TRUE(urls != nullptr);

  int url_count = 0;
  for (auto i(urls->begin()); i != urls->end(); ++i) {
    url_count++;
    ASSERT_EQ(template_urls_[1].get(), *i);
  }
  ASSERT_EQ(1, url_count);
}

TEST_F(SearchHostToURLsMapTest, GetsBestTemplateURLForKnownHost) {
  // The second one should be slightly newer.
  const TemplateURL* found_url = provider_map_->GetTemplateURLForHost(host_);
  ASSERT_TRUE(found_url == template_urls_[1].get());

  TemplateURLData data;
  data.SetURL("http://" + host_ + "/path1");
  // Make the new TemplateURL "better" by having it created by policy.
  data.created_by_policy =
      TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;

  TemplateURL new_t_url(data);
  provider_map_->Add(&new_t_url, SearchTermsData());

  found_url = provider_map_->GetTemplateURLForHost(host_);
  EXPECT_EQ(found_url, &new_t_url) << "We should have found the new better "
                                      "TemplateURL that was created by policy.";
}

TEST_F(SearchHostToURLsMapTest, GetTemplateURLForUnknownHost) {
  const TemplateURL* found_url =
      provider_map_->GetTemplateURLForHost("a" + host_);
  ASSERT_TRUE(found_url == nullptr);
}

TEST_F(SearchHostToURLsMapTest, GetURLsForKnownHost) {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host_);
  ASSERT_TRUE(urls != nullptr);

  for (const auto& url : template_urls_)
    EXPECT_NE(urls->end(), urls->find(url.get()));
}

TEST_F(SearchHostToURLsMapTest, GetURLsForUnknownHost) {
  const SearchHostToURLsMap::TemplateURLSet* urls =
      provider_map_->GetURLsForHost("a" + host_);
  ASSERT_TRUE(urls == nullptr);
}
