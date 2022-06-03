// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_HISTORY_UI_H_

#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace media_history {
class MediaHistoryKeyedService;
}  // namespace media_history

// The UI for chrome://media-history.
class MediaHistoryUI : public ui::MojoWebUIController,
                       public media_history::mojom::MediaHistoryStore {
 public:
  explicit MediaHistoryUI(content::WebUI* web_ui);
  MediaHistoryUI(const MediaHistoryUI&) = delete;
  MediaHistoryUI& operator=(const MediaHistoryUI&) = delete;
  ~MediaHistoryUI() override;

  // Instantiates the implementor of the MediaEngagementScoreDetailsProvider
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<media_history::mojom::MediaHistoryStore> pending);

  // media::mojom::MediaHistoryStore:
  void GetMediaHistoryStats(GetMediaHistoryStatsCallback callback) override;
  void GetMediaHistoryOriginRows(
      GetMediaHistoryOriginRowsCallback callback) override;
  void GetMediaHistoryPlaybackRows(
      GetMediaHistoryPlaybackRowsCallback callback) override;
  void GetMediaHistoryPlaybackSessionRows(
      GetMediaHistoryPlaybackSessionRowsCallback callback) override;

 private:
  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  mojo::ReceiverSet<media_history::mojom::MediaHistoryStore> receivers_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_HISTORY_UI_H_
