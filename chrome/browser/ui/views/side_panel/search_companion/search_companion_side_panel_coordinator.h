// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

class Browser;
class PrefService;
class Profile;

// SearchCompanionSidePanelCoordinator handles the creation and registration of
// the search companion SidePanelEntry.
class SearchCompanionSidePanelCoordinator
    : public BrowserUserData<SearchCompanionSidePanelCoordinator>,
      public TabStripModelObserver,
      public TemplateURLServiceObserver {
 public:
  explicit SearchCompanionSidePanelCoordinator(Browser* browser);
  SearchCompanionSidePanelCoordinator(
      const SearchCompanionSidePanelCoordinator&) = delete;
  SearchCompanionSidePanelCoordinator& operator=(
      const SearchCompanionSidePanelCoordinator&) = delete;
  ~SearchCompanionSidePanelCoordinator() override;

  // If `include_runtime_checks` is true, then the method returns true if the
  // runtime checks also return true.
  static bool IsSupported(Profile* profile, bool include_runtime_checks);

  bool Show(SidePanelOpenTrigger side_panel_open_trigger);
  BrowserView* GetBrowserView() const;
  std::u16string GetTooltipForToolbarButton() const;

  std::u16string name() const { return name_; }
  const gfx::VectorIcon& icon() { return *icon_; }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // For metrics only. Notifies the companion of the side panel open trigger.
  void NotifyCompanionOfSidePanelOpenTrigger(
      absl::optional<SidePanelOpenTrigger> side_panel_open_trigger);

 private:
  friend class BrowserUserData<SearchCompanionSidePanelCoordinator>;

  void CreateAndRegisterEntriesForExistingWebContents(
      TabStripModel* tab_strip_model);
  void DeregisterEntriesForExistingWebContents(TabStripModel* tab_strip_model);

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Updates CSC availability in Side Panel.
  void UpdateCompanionAvailabilityInSidePanel();

  // Called if there is a change in the state of policy pref.
  void OnPolicyPrefChanged();

  // Returns true if CSC runtime checks pass.
  bool DoCompanionRuntimeChecksPass() const;

  raw_ptr<Browser> browser_;
  std::u16string name_;
  const raw_ref<const gfx::VectorIcon, ExperimentalAsh> icon_;
  raw_ptr<PrefService> pref_service_;
  bool dsp_is_google_ = false;
  bool csc_enabled_via_policy_ = false;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_
