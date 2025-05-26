// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_register.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class BrowserWindowInterface;
class TabStripModel;

// TODO (crbug.com/409086859). See bug for dd.
// tabs_api::mojom::TabStripController is an experimental TabStrip Api between
// any view and the TabStripModel.
class TabStripServiceImpl : public tabs_api::mojom::TabStripService,
                            public TabStripModelObserver,
                            public TabStripServiceRegister {
 public:
  TabStripServiceImpl(BrowserWindowInterface* browser,
                      TabStripModel* tab_strip_model);
  TabStripServiceImpl(
      std::unique_ptr<tabs_api::BrowserAdapter> browser_adapter,
      std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_adapter);
  TabStripServiceImpl(
      std::unique_ptr<tabs_api::BrowserAdapter> browser_adapter,
      std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_adapter,
      std::unique_ptr<tabs_api::events::TabStripEventRecorder> recorder);
  TabStripServiceImpl(const TabStripServiceImpl&&) = delete;
  TabStripServiceImpl& operator=(const TabStripServiceImpl&) = delete;
  ~TabStripServiceImpl() override;

  // TabStripServiceregister overrides
  void Accept(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) override;

  // tabs_api::mojom::TabStripService overrides
  void GetTabs(GetTabsCallback callback) override;
  void GetTab(const tabs_api::TabId& id, GetTabCallback callback) override;
  void CreateTabAt(tabs_api::mojom::PositionPtr pos,
                   const std::optional<GURL>& url,
                   CreateTabAtCallback callback) override;
  void CloseTabs(const std::vector<tabs_api::TabId>& ids,
                 CloseTabsCallback callback) override;
  void ActivateTab(const tabs_api::TabId& id,
                   ActivateTabCallback callback) override;

  static base::PassKey<TabStripServiceImpl> get_passkey_for_testing() {
    return base::PassKey<TabStripServiceImpl>();
  }

 private:
  void BroadcastEvent(tabs_api::events::Event& event) const;

  std::unique_ptr<tabs_api::BrowserAdapter> browser_adapter_;
  std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_model_adapter_;
  std::unique_ptr<tabs_api::events::TabStripEventRecorder> recorder_;

  mojo::ReceiverSet<tabs_api::mojom::TabStripService> clients_;
  mojo::AssociatedRemoteSet<tabs_api::mojom::TabsObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_
