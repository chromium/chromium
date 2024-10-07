// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class Browser;

namespace tabs {
class TabModel;
}  // namespace tabs

class TabOrganizationSession : public TabOrganization::Observer {
 public:
  // TODO(dpenning): make this a base::Token.
  using ID = int;
  using TabOrganizations = std::vector<std::unique_ptr<TabOrganization>>;

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnTabOrganizationSessionUpdated(
        const TabOrganizationSession* session) {}
    virtual void OnTabOrganizationSessionDestroyed(
        TabOrganizationSession::ID session_id) {}
  };

  TabOrganizationSession();
  explicit TabOrganizationSession(
      std::unique_ptr<TabOrganizationRequest> request,
      TabOrganizationEntryPoint entrypoint = TabOrganizationEntryPoint::kNone,
      const tabs::TabModel* base_session_tab = nullptr);
  ~TabOrganizationSession() override;

  const TabOrganizationRequest* request() const { return request_.get(); }
  const TabOrganizations& tab_organizations() const {
    return tab_organizations_;
  }
  ID session_id() const { return session_id_; }
  std::u16string feedback_id() const { return feedback_id_; }
  optimization_guide::proto::UserFeedback feedback() const { return feedback_; }
  const tabs::TabModel* base_session_tab() const { return base_session_tab_; }

  static std::unique_ptr<TabOrganizationSession> CreateSessionForBrowser(
      const Browser* browser,
      const TabOrganizationEntryPoint entrypoint,
      const tabs::TabModel* base_session_tab = nullptr);

  const TabOrganization* GetNextTabOrganization() const;
  TabOrganization* GetNextTabOrganization();

  void StartRequest();

  void AddOrganizationForTesting(
      std::unique_ptr<TabOrganization> tab_organization) {
    tab_organizations_.emplace_back(std::move(tab_organization));
  }

  // Returns true if the request is not completed or there are still actions
  // that need to be taken on organizations.
  bool IsComplete() const;

  void SetFeedback(optimization_guide::proto::UserFeedback feedback) {
    feedback_ = feedback;
  }

  void AddObserver(Observer* new_observer);
  void RemoveObserver(Observer* new_observer);

  // TabOrganization::Observer
  void OnTabOrganizationUpdated(const TabOrganization* organization) override;
  void OnTabOrganizationDestroyed(TabOrganization::ID organization_id) override;

 private:
  // Notifies observers of the tab data that it has been updated.
  void NotifyObserversOfUpdate();

  // Checks whether there is a response, and if so calls Populate functions.
  // Notifies observers that the session has been updated.
  void OnRequestResponse(TabOrganizationResponse* response);

  // TODO: Remove once the full UI flow is implemented.
  void PopulateAndCreate(TabOrganizationResponse* response);

  // Fills in the organizations from the request. Called when the request
  // completes.
  void PopulateOrganizations(TabOrganizationResponse* response);

  std::unique_ptr<TabOrganizationRequest> request_;
  TabOrganizations tab_organizations_;
  ID session_id_;
  std::u16string feedback_id_;

  // Represents whether the user has provided feedback via the thumbs UI.
  optimization_guide::proto::UserFeedback feedback_ =
      optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;

  // Entry point used to create the session. Used for logging.
  TabOrganizationEntryPoint entrypoint_;

  // Active tab web contents tied to the session, if any.
  raw_ptr<const tabs::TabModel> base_session_tab_;

  base::ObserverList<Observer>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
