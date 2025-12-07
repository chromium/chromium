// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/test_matchers.h"

#include "components/sessions/core/session_id.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {
namespace {

using testing::_;
using testing::ContainerEq;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::PrintToString;

class MatchesHeaderMatcher
    : public MatcherInterface<const sync_pb::SessionSpecifics&> {
 public:
  MatchesHeaderMatcher(Matcher<std::string> session_tag,
                       Matcher<base::Time> session_start_time,
                       Matcher<std::vector<int>> window_ids,
                       Matcher<std::vector<int>> tab_ids)
      : session_tag_(session_tag),
        session_start_time_(session_start_time),
        window_ids_(window_ids),
        tab_ids_(tab_ids) {}

  bool MatchAndExplain(const sync_pb::SessionSpecifics& actual,
                       MatchResultListener* listener) const override {
    if (!actual.has_header()) {
      *listener << " which is not a header entity";
      return false;
    }
    if (actual.tab_node_id() != -1) {
      *listener << " which has a valid tab node ID: " << actual.tab_node_id();
      return false;
    }
    if (!session_tag_.MatchAndExplain(actual.session_tag(), listener)) {
      *listener << " which contains an unexpected session tag: "
                << actual.session_tag();
      return false;
    }
    base::Time actual_start_time = base::Time::FromMillisecondsSinceUnixEpoch(
        actual.header().session_start_time_unix_epoch_millis());
    if (!session_start_time_.MatchAndExplain(actual_start_time, listener)) {
      *listener << " which contains an unexpected start time: "
                << actual_start_time;
      return false;
    }
    std::vector<int> actual_window_ids;
    std::vector<int> actual_tab_ids;
    for (const auto& window : actual.header().window()) {
      actual_window_ids.push_back(window.window_id());
      actual_tab_ids.insert(actual_tab_ids.end(), window.tab().begin(),
                            window.tab().end());
    }
    if (!window_ids_.MatchAndExplain(actual_window_ids, listener)) {
      *listener << " which contains unexpected windows: "
                << PrintToString(actual_window_ids);
      return false;
    }
    if (!tab_ids_.MatchAndExplain(actual_tab_ids, listener)) {
      *listener << " which contains unexpected tabs: "
                << PrintToString(actual_tab_ids);
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "matches expected header";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not match expected header";
  }

 private:
  Matcher<std::string> session_tag_;
  Matcher<base::Time> session_start_time_;
  Matcher<std::vector<int>> window_ids_;
  Matcher<std::vector<int>> tab_ids_;
};

class MatchesTabMatcher
    : public MatcherInterface<const sync_pb::SessionSpecifics&> {
 public:
  MatchesTabMatcher(Matcher<std::string> session_tag,
                    Matcher<int> window_id,
                    Matcher<int> tab_id,
                    Matcher<int> tab_node_id,
                    Matcher<std::vector<std::string>> urls)
      : session_tag_(session_tag),
        window_id_(window_id),
        tab_id_(tab_id),
        tab_node_id_(tab_node_id),
        urls_(urls) {}

  bool MatchAndExplain(const sync_pb::SessionSpecifics& actual,
                       MatchResultListener* listener) const override {
    if (!actual.has_tab()) {
      *listener << " which is not a tab entity";
      return false;
    }
    if (!session_tag_.MatchAndExplain(actual.session_tag(), listener)) {
      *listener << " which contains an unexpected session tag: "
                << actual.session_tag();
      return false;
    }
    if (!window_id_.MatchAndExplain(actual.tab().window_id(), listener)) {
      *listener << " which contains an unexpected window ID: "
                << actual.tab().window_id();
      return false;
    }
    if (!tab_id_.MatchAndExplain(actual.tab().tab_id(), listener)) {
      *listener << " which contains an unexpected tab ID: "
                << actual.tab().tab_id();
      return false;
    }
    if (!tab_node_id_.MatchAndExplain(actual.tab_node_id(), listener)) {
      *listener << " which contains an unexpected tab node ID: "
                << actual.tab_node_id();
      return false;
    }
    std::vector<std::string> actual_urls;
    for (const sync_pb::TabNavigation& navigation : actual.tab().navigation()) {
      actual_urls.push_back(navigation.virtual_url());
    }
    if (!urls_.MatchAndExplain(actual_urls, listener)) {
      *listener << " which contains unexpected navigation URLs";
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "matches expected tab";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not match expected tab";
  }

 private:
  Matcher<std::string> session_tag_;
  Matcher<int> window_id_;
  Matcher<int> tab_id_;
  Matcher<int> tab_node_id_;
  Matcher<std::vector<std::string>> urls_;
};

class MatchesSyncedSessionMatcher
    : public MatcherInterface<const SyncedSession*> {
 public:
  MatchesSyncedSessionMatcher(
      Matcher<std::string> session_tag,
      Matcher<std::map<int, std::vector<int>>> window_id_to_tabs)
      : session_tag_(session_tag), window_id_to_tabs_(window_id_to_tabs) {}

  bool MatchAndExplain(const SyncedSession* actual,
                       MatchResultListener* listener) const override {
    if (!actual) {
      *listener << " which is null";
      return false;
    }
    if (!session_tag_.MatchAndExplain(actual->GetSessionTag(), listener)) {
      *listener << " which contains an unexpected session tag: "
                << actual->GetSessionTag();
      return false;
    }

    std::map<int, std::vector<int>> actual_window_id_to_tabs;
    for (const auto& [actual_window_id, actual_window] : actual->windows) {
      if (actual_window_id != actual_window->wrapped_window.window_id) {
        *listener << " which has an inconsistent window representation";
        return false;
      }
      actual_window_id_to_tabs.emplace(actual_window_id.id(),
                                       std::vector<int>());
      for (const auto& tab : actual_window->wrapped_window.tabs) {
        actual_window_id_to_tabs[actual_window_id.id()].push_back(
            tab->tab_id.id());
      }
    }

    if (!window_id_to_tabs_.MatchAndExplain(actual_window_id_to_tabs,
                                            listener)) {
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "matches expected synced session";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not match expected synced session";
  }

 private:
  Matcher<std::string> session_tag_;
  Matcher<std::map<int, std::vector<int>>> window_id_to_tabs_;
};

}  // namespace

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<base::Time> session_start_time,
    testing::Matcher<std::vector<int>> window_ids,
    testing::Matcher<std::vector<int>> tab_ids) {
  return testing::MakeMatcher(new MatchesHeaderMatcher(
      session_tag, session_start_time, window_ids, tab_ids));
}

Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    Matcher<std::string> session_tag,
    Matcher<std::vector<int>> window_ids,
    Matcher<std::vector<int>> tab_ids) {
  return MatchesHeader(session_tag, _, window_ids, tab_ids);
}

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    base::Time session_start_time,
    const std::vector<int>& window_ids,
    const std::vector<int>& tab_ids) {
  return MatchesHeader(session_tag, Eq(session_start_time),
                       ElementsAreArray(window_ids), ElementsAreArray(tab_ids));
}

Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    Matcher<std::string> session_tag,
    const std::vector<int>& window_ids,
    const std::vector<int>& tab_ids) {
  return MatchesHeader(session_tag, ElementsAreArray(window_ids),
                       ElementsAreArray(tab_ids));
}

Matcher<const sync_pb::SessionSpecifics&> MatchesTab(
    Matcher<std::string> session_tag,
    Matcher<int> window_id,
    Matcher<int> tab_id,
    Matcher<int> tab_node_id,
    Matcher<std::vector<std::string>> urls) {
  return testing::MakeMatcher(
      new MatchesTabMatcher(session_tag, window_id, tab_id, tab_node_id, urls));
}

Matcher<const sync_pb::SessionSpecifics&> MatchesTab(
    Matcher<std::string> session_tag,
    Matcher<int> window_id,
    Matcher<int> tab_id,
    Matcher<int> tab_node_id,
    const std::vector<std::string>& urls) {
  return MatchesTab(session_tag, window_id, tab_id, tab_node_id,
                    ElementsAreArray(urls));
}

Matcher<const SyncedSession*> MatchesSyncedSession(
    Matcher<std::string> session_tag,
    Matcher<std::map<int, std::vector<int>>> window_id_to_tabs) {
  return testing::MakeMatcher(
      new MatchesSyncedSessionMatcher(session_tag, window_id_to_tabs));
}

Matcher<const SyncedSession*> MatchesSyncedSession(
    Matcher<std::string> session_tag,
    const std::map<int, std::vector<int>>& window_id_to_tabs) {
  return MatchesSyncedSession(session_tag, ContainerEq(window_id_to_tabs));
}

}  // namespace sync_sessions
