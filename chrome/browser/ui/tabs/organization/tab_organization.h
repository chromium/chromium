// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class TabOrganization : public TabData::Observer {
 public:
  using ID = int;
  using TabDatas = std::vector<std::unique_ptr<TabData>>;

  // Used to display the current name of the organization by either indexing
  // into the names_ list (the size_t) or providing a custom name |u16string|.
  using CurrentName = absl::variant<size_t, std::u16string>;

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnTabOrganizationUpdated(const TabOrganization* organization) {
    }
    virtual void OnTabOrganizationDestroyed(
        TabOrganization::ID organization_id) {}
  };

  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "TabOrganizationUserChoice" in src/tools/metrics/histograms/enums.xml.
  enum class UserChoice {
    kNoChoice = 0,
    kAccepted = 1,
    kRejected = 2,
    kMaxValue = kRejected,
  };

  TabOrganization(TabDatas tab_datas,
                  std::vector<std::u16string> names,
                  CurrentName current_name = 0u,
                  UserChoice choice = UserChoice::kNoChoice);
  ~TabOrganization() override;

  const TabDatas& tab_datas() const { return tab_datas_; }
  const std::vector<std::u16string>& names() const { return names_; }
  const CurrentName& current_name() const { return current_name_; }
  UserChoice choice() const { return choice_; }
  optimization_guide::proto::UserFeedback feedback() const { return feedback_; }
  ID organization_id() const { return organization_id_; }
  const std::u16string GetDisplayName() const;
  int GetTabRemovedCount() const { return user_removed_tab_ids_.size(); }

  bool IsValidForOrganizing() const;

  void AddObserver(Observer* new_observer);
  void RemoveObserver(Observer* new_observer);

  void AddTabData(std::unique_ptr<TabData> tab_data);
  void RemoveTabData(TabData::TabID id);
  void SetCurrentName(CurrentName new_current_name);
  void SetFeedback(optimization_guide::proto::UserFeedback feedback);
  void Accept();
  void Reject();

  // TabData::Observer
  void OnTabDataUpdated(const TabData* tab_data) override;
  void OnTabDataDestroyed(TabData::TabID tab_id) override;

 private:
  // Notifies observers of the tab data that it has been updated.
  void NotifyObserversOfUpdate();

  // The tabs that are currently included in the organization. When accepted,
  // they will be organized in the tabstrip.
  TabDatas tab_datas_;

  // The tab ids that have been removed by the user after the organization was
  // instantiated.
  std::vector<TabData::TabID> user_removed_tab_ids_;

  // The list of suggested names for the organization. If the |current_name_| is
  // a size_t then it refers to an index in the |names_| vector.
  std::vector<std::u16string> names_;

  // The currently set name for the organization. defaults to the first name in
  // the list of names, but if the user changes to a custom name, it will be
  // represented as a u16string.
  CurrentName current_name_ = 0u;

  // What the user has decided to do with the Organization. If a user doesnt
  // Interact with the organization then this will have the value kNoChoice.
  // Once the user has interacted this will become either kAccepted or
  // kRejected. Set only via the Accept() and Reject() methods.
  UserChoice choice_ = UserChoice::kNoChoice;

  // A separate feedback mechanism, represents whether the user has provided
  // feedback via the thumbs UI.
  optimization_guide::proto::UserFeedback feedback_ =
      optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;

  // a monotonically increasing ID to refer to the organization in the
  // TabOrganizationSession.
  ID organization_id_;

  // A flag that forces the tab organization to be marked as invalid.
  bool invalidated_by_tab_change_ = false;

  base::ObserverList<Observer>::Unchecked observers_;
  base::WeakPtrFactory<TabOrganization> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_H_
