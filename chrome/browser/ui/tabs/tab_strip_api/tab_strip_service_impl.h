// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserWindowInterface;
class TabStripModel;

// TODO (crbug.com/409086859). See bug for dd.
// tabs_api::mojom::TabStripController is an experimental TabStrip Api between
// any view and the TabStripModel.
class TabStripServiceImpl : public tabs_api::mojom::TabStripService,
                            public TabStripModelObserver {
 public:
  explicit TabStripServiceImpl(BrowserWindowInterface* browser,
                               TabStripModel* tab_strip_model);
  TabStripServiceImpl(const TabStripServiceImpl&) = delete;
  TabStripServiceImpl& operator=(const TabStripServiceImpl&) = delete;
  ~TabStripServiceImpl() override;

  void AddObserver(tabs_api::mojom::TabsObserver* observer);
  void RemoveObserver(tabs_api::mojom::TabsObserver* observer);

  // tabs_api::mojom::TabStripService overrides
  void CreateTabAt(tabs_api::mojom::PositionPtr pos,
                   const std::optional<GURL>& url,
                   CreateTabAtCallback callback) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 protected:
  // Helper method used to add tab. This is primarily to mock for unit tests
  // until there is a better way to mock chrome::AddAndReturnTabAt.
  virtual content::WebContents* AddTabAt(const GURL& url, int index);

 private:
  void OnTabStripModelChangeAdded(const TabStripModelChange::Insert& change);

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripModel> model_;

  // Reminder that ObserverList is currently not thread-safe.
  // If usage expands to different threads, this needs to transition to
  // ObserverListThreadSafe.
  base::ObserverList<tabs_api::mojom::TabsObserver, true>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_
