// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_test_util.h"

#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/fake_distiller.h"

using leveldb_proto::test::FakeDB;

namespace dom_distiller {
namespace test {
namespace util {

namespace {

std::vector<ArticleEntry> EntryMapToList(
    const FakeDB<ArticleEntry>::EntryMap& entries) {
  std::vector<ArticleEntry> entry_list;
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    entry_list.push_back(it->second);
  }
  return entry_list;
}
}  // namespace

ObserverUpdatesMatcher::ObserverUpdatesMatcher(
    const std::vector<DomDistillerObserver::ArticleUpdate>& expected_updates)
    : expected_updates_(expected_updates) {}

bool ObserverUpdatesMatcher::MatchAndExplain(
    const std::vector<DomDistillerObserver::ArticleUpdate>& actual_updates,
    testing::MatchResultListener* listener) const {
  if (actual_updates.size() != expected_updates_.size()) {
    *listener << "Incorrect number of updates. Expected: "
              << expected_updates_.size() << " got: " << actual_updates.size();
    return false;
  }
  std::vector<DomDistillerObserver::ArticleUpdate>::const_iterator expected,
      actual;
  for (expected = expected_updates_.begin(), actual = actual_updates.begin();
       expected != expected_updates_.end(); ++expected, ++actual) {
    if (expected->entry_id != actual->entry_id) {
      *listener << " Mismatched entry id. Expected: " << expected->entry_id
                << " actual: " << actual->entry_id;
      return false;
    }
    if (expected->update_type != actual->update_type) {
      *listener << " Mismatched update for entryid:" << expected->entry_id
                << ". Expected: " << expected->update_type
                << " actual: " << actual->update_type;
      return false;
    }
  }
  return true;
}

void ObserverUpdatesMatcher::DescribeUpdates(std::ostream* os) const {
  bool start = true;
  for (auto i = expected_updates_.begin(); i != expected_updates_.end(); ++i) {
    if (start) {
      start = false;
    } else {
      *os << ", ";
    }
    *os << "( EntryId: " << i->entry_id << ", UpdateType: " << i->update_type
        << " )";
  }
}

void ObserverUpdatesMatcher::DescribeTo(std::ostream* os) const {
  *os << " has updates: { ";
  DescribeUpdates(os);
  *os << "}";
}
void ObserverUpdatesMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << " does not have updates: { ";
  DescribeUpdates(os);
  *os << "}";
}

testing::Matcher<const std::vector<DomDistillerObserver::ArticleUpdate>&>
HasExpectedUpdates(
    const std::vector<DomDistillerObserver::ArticleUpdate>& expected_updates) {
  return testing::MakeMatcher(new ObserverUpdatesMatcher(expected_updates));
}

// static
DomDistillerStore* CreateStoreWithFakeDB(
    FakeDB<ArticleEntry>* fake_db,
    const FakeDB<ArticleEntry>::EntryMap& store_model) {
  return new DomDistillerStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<ArticleEntry>>(fake_db),
      EntryMapToList(store_model), FakeDB<ArticleEntry>::DirectoryForTestDB());
}

}  // namespace util
}  // namespace test
}  // namespace dom_distiller
