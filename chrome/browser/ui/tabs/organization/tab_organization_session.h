// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"

class Browser;
namespace Content {
class WebContents;
}

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
      TabOrganizationEntryPoint entrypoint = TabOrganizationEntryPoint::NONE);
  ~TabOrganizationSession() override;

  const TabOrganizationRequest* request() const { return request_.get(); }
  const TabOrganizations& tab_organizations() const {
    return tab_organizations_;
  }
  ID session_id() const { return session_id_; }
  std::u16string feedback_id() const { return feedback_id_; }

  static std::unique_ptr<TabOrganizationSession> CreateSessionForBrowser(
      const Browser* browser,
      const content::WebContents* base_session_webcontents = nullptr);

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

  // Entry point used to create the session. Used for logging.
  TabOrganizationEntryPoint entrypoint_;

  base::ObserverList<Observer>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
