// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_PAGE_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/read_later/read_later.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class Clock;
}

class Browser;
class GURL;
class ReadingListEntry;
class ReadingListModel;

class ReadLaterPageHandler : public read_later::mojom::PageHandler {
 public:
  ReadLaterPageHandler(
      mojo::PendingReceiver<read_later::mojom::PageHandler> receiver,
      mojo::PendingRemote<read_later::mojom::Page> page);
  ReadLaterPageHandler(const ReadLaterPageHandler&) = delete;
  ReadLaterPageHandler& operator=(const ReadLaterPageHandler&) = delete;
  ~ReadLaterPageHandler() override;

  // read_later::mojom::PageHandler:
  void GetReadLaterEntries(GetReadLaterEntriesCallback callback) override;
  void OpenSavedEntry(const GURL& url) override;
  void UpdateReadStatus(const GURL& url, bool read) override;
  void RemoveEntry(const GURL& url) override;

 private:
  // Gets the reading list entry data used for displaying to the user and
  // triggering actions.
  read_later::mojom::ReadLaterEntryPtr GetEntryData(
      const ReadingListEntry* entry);

  // Converts |last_update_time| from microseconds since epoch in Unix-like
  // system (Jan 1, 1970), since this is how ReadingListEntry's |update_time| is
  // stored, to a localized representation as a delay (e.g. "5 minutes ago").
  std::string GetTimeSinceLastUpdate(int64_t last_update_time);

  mojo::Receiver<read_later::mojom::PageHandler> receiver_;
  mojo::Remote<read_later::mojom::Page> page_;
  Browser* const browser_;

  base::Clock* clock_;

  ReadingListModel* reading_list_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_PAGE_HANDLER_H_
