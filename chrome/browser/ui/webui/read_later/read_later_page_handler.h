// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_PAGE_HANDLER_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/read_later/read_later.mojom.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class Clock;
}

namespace content {
class WebContents;
class WebUI;
}  // namespace content

class GURL;
class ReadLaterUI;
class ReadingListEntry;

class ReadLaterPageHandler : public read_later::mojom::PageHandler,
                             public ReadingListModelObserver {
 public:
  ReadLaterPageHandler(
      mojo::PendingReceiver<read_later::mojom::PageHandler> receiver,
      mojo::PendingRemote<read_later::mojom::Page> page,
      ReadLaterUI* read_later_ui,
      content::WebUI* web_ui);
  ReadLaterPageHandler(const ReadLaterPageHandler&) = delete;
  ReadLaterPageHandler& operator=(const ReadLaterPageHandler&) = delete;
  ~ReadLaterPageHandler() override;

  // read_later::mojom::PageHandler:
  void GetReadLaterEntries(GetReadLaterEntriesCallback callback) override;
  void OpenURL(const GURL& url,
               bool mark_as_read,
               ui::mojom::ClickModifiersPtr click_modifiers) override;
  void UpdateReadStatus(const GURL& url, bool read) override;
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

  const absl::optional<GURL> GetActiveTabURL();
  void SetActiveTabURL(const GURL& url);

  void set_web_contents_for_testing(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  // Gets the reading list entry data used for displaying to the user and
  // triggering actions.
  read_later::mojom::ReadLaterEntryPtr GetEntryData(
      const ReadingListEntry* entry);

  // Returns the lists for the read/unread entries.
  read_later::mojom::ReadLaterEntriesByStatusPtr
  CreateReadLaterEntriesByStatusData();

  // Converts |last_update_time| from microseconds since epoch in Unix-like
  // system (Jan 1, 1970), since this is how ReadingListEntry's |update_time| is
  // stored, to a localized representation as a delay (e.g. "5 minutes ago").
  std::string GetTimeSinceLastUpdate(int64_t last_update_time);

  void UpdateCurrentPageActionButton();

  mojo::Receiver<read_later::mojom::PageHandler> receiver_;
  mojo::Remote<read_later::mojom::Page> page_;
  // ReadLaterPageHandler is owned by |read_later_ui_| and so we expect
  // |read_later_ui_| to remain valid for the lifetime of |this|.
  ReadLaterUI* const read_later_ui_;
  content::WebUI* const web_ui_;
  content::WebContents* web_contents_;

  absl::optional<GURL> active_tab_url_;
  read_later::mojom::CurrentPageActionButtonState
      current_page_action_button_state_ =
          read_later::mojom::CurrentPageActionButtonState::kDisabled;

  base::Clock* clock_;

  ReadingListModel* reading_list_model_ = nullptr;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_PAGE_HANDLER_H_
