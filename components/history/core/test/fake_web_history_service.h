// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_FAKE_WEB_HISTORY_SERVICE_H_
#define COMPONENTS_HISTORY_CORE_TEST_FAKE_WEB_HISTORY_SERVICE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/web_history_service.h"
#include "url/gurl.h"

namespace history {

// A fake WebHistoryService for testing.
//
// Use `AddSyncedVisit` to fill the fake server-side database of synced visits.
// Use `SetupFakeResponse` to influence whether the requests should succeed
// or fail, and with which error code.
//
// Note: The behavior of this class is only defined for some WebHistoryService
// queries. If needed, improve FakeRequest::GetResponseBody() to simulate
// responses for other queries.
//
// TODO(msramek): This class might need its own set of tests.
class FakeWebHistoryService : public WebHistoryService {
 public:
  FakeWebHistoryService();

  FakeWebHistoryService(const FakeWebHistoryService&) = delete;
  FakeWebHistoryService& operator=(const FakeWebHistoryService&) = delete;

  ~FakeWebHistoryService() override;

  // Sets up the behavior of the fake response returned when calling
  // `WebHistoryService::QueryHistory`; whether it will succeed, and with
  // which response code.
  void SetupFakeResponse(bool emulate_success, int emulate_response_code);

  // Adds a fake visit.
  void AddSyncedVisit(const std::string& url,
                      base::Time timestamp,
                      const std::string& icon_url = std::string(""));

  // Clears all fake visits.
  void ClearSyncedVisits();

  // Get and set the fake state of web and app activity.
  bool IsWebAndAppActivityEnabled();
  void SetWebAndAppActivityEnabled(bool enabled);

  // Get and set the fake state of other forms of browsing history.
  bool AreOtherFormsOfBrowsingHistoryPresent();
  void SetOtherFormsOfBrowsingHistoryPresent(bool present);

 protected:
  struct Visit {
    Visit(const std::string& url,
          base::Time timestamp,
          const std::string& icon_url);
    std::string url;
    base::Time timestamp;
    std::string icon_url;
  };

  // Returns up to `count` results from `visits_` between `begin` and `end`.
  // Results are sorted from most recent to least recent, prioritizing more
  // recent results when some need to be omitted. `more_results_left` will be
  // set to true only if there are results from `visits_` that were not included
  // because of `count` limitations, but were also within time range. Virtual to
  // allow subclasses to modify.
  virtual std::vector<FakeWebHistoryService::Visit> GetVisitsBetween(
      base::Time begin,
      base::Time end,
      size_t count,
      bool* more_results_left);

 private:
  class FakeRequest;

  base::Time GetTimeForKeyInQuery(const GURL& url, const std::string& key);

  // WebHistoryService implementation.
  Request* CreateRequest(const GURL& url,
                         CompletionCallback callback,
                         const net::PartialNetworkTrafficAnnotationTag&
                             partial_traffic_annotation) override;

  // Parameters for the fake request.
  bool emulate_success_;
  int emulate_response_code_;

  // States of serverside corpora.
  bool web_and_app_activity_enabled_;
  bool other_forms_of_browsing_history_present_;

  // Fake visits storage.
  std::vector<Visit> visits_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_FAKE_WEB_HISTORY_SERVICE_H_
