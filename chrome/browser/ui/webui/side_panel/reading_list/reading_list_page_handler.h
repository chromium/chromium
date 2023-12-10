// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/models/simple_menu_model.h"

namespace base {
class Clock;
}

namespace content {
class WebContents;
class WebUI;
}  // namespace content

class Browser;
class GURL;
class ReadingListUI;
class ReadingListEntry;

class ReadingListPageHandler : public reading_list::mojom::PageHandler,
                               public ReadingListModelObserver {
 public:
  ReadingListPageHandler(
      mojo::PendingReceiver<reading_list::mojom::PageHandler> receiver,
      mojo::PendingRemote<reading_list::mojom::Page> page,
      ReadingListUI* reading_list_ui,
      content::WebUI* web_ui);
  ReadingListPageHandler(const ReadingListPageHandler&) = delete;
  ReadingListPageHandler& operator=(const ReadingListPageHandler&) = delete;
  ~ReadingListPageHandler() override;

  // reading_list::mojom::PageHandler:
  void GetReadLaterEntries(GetReadLaterEntriesCallback callback) override;
  void OpenURL(const GURL& url,
               bool mark_as_read,
               ui::mojom::ClickModifiersPtr click_modifiers) override;
  void UpdateReadStatus(const GURL& url, bool read) override;
  void MarkCurrentTabAsRead() override;
  void AddCurrentTab() override;
  void RemoveEntry(const GURL& url) override;
  void ShowContextMenuForURL(const GURL& url, int32_t x, int32_t y) override;
  void UpdateCurrentPageActionButtonState() override;
  void ShowUI() override;
  void CloseUI() override;

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override {}
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;

  const std::optional<GURL> GetActiveTabURL();
  void SetActiveTabURL(const GURL& url);

  void set_web_contents_for_testing(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  reading_list::mojom::CurrentPageActionButtonState
  GetCurrentPageActionButtonStateForTesting() {
    return current_page_action_button_state_;
  }

  std::unique_ptr<ui::SimpleMenuModel> GetItemContextMenuModelForTesting(
      Browser* browser,
      ReadingListModel* reading_list_model,
      GURL url);

 private:
  // Gets the reading list entry data used for displaying to the user and
  // triggering actions.
  reading_list::mojom::ReadLaterEntryPtr GetEntryData(
      const ReadingListEntry* entry);

  // Returns the lists for the read/unread entries.
  reading_list::mojom::ReadLaterEntriesByStatusPtr
  CreateReadLaterEntriesByStatusData();

  // Converts |last_update_time| from microseconds since epoch in Unix-like
  // system (Jan 1, 1970), since this is how ReadingListEntry's |update_time| is
  // stored, to a localized representation as a delay (e.g. "5 minutes ago").
  std::string GetTimeSinceLastUpdate(int64_t last_update_time);

  void UpdateCurrentPageActionButton();

  mojo::Receiver<reading_list::mojom::PageHandler> receiver_;
  mojo::Remote<reading_list::mojom::Page> page_;
  // ReadingListPageHandler is owned by |reading_list_ui_| and so we expect
  // |reading_list_ui_| to remain valid for the lifetime of |this|.
  const raw_ptr<ReadingListUI> reading_list_ui_;
  const raw_ptr<content::WebUI> web_ui_;
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

  std::optional<GURL> active_tab_url_;
  reading_list::mojom::CurrentPageActionButtonState
      current_page_action_button_state_ =
          reading_list::mojom::CurrentPageActionButtonState::kDisabled;

  raw_ptr<base::Clock> clock_;

  raw_ptr<ReadingListModel> reading_list_model_ = nullptr;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_PAGE_HANDLER_H_
