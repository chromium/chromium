// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BATCH_UPLOAD_PROMO_BATCH_UPLOAD_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BATCH_UPLOAD_PROMO_BATCH_UPLOAD_PROMO_HANDLER_H_

#include "base/scoped_observation.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/js/batch_upload_promo/batch_upload_promo.mojom.h"

class Profile;
class BatchUploadService;

namespace content {
class WebContents;
}

class BatchUploadPromoHandler : public batch_upload_promo::mojom::PageHandler,
                                public syncer::SyncServiceObserver {
 public:
  BatchUploadPromoHandler(
      mojo::PendingReceiver<batch_upload_promo::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<batch_upload_promo::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents);
  BatchUploadPromoHandler(const BatchUploadPromoHandler&) = delete;
  BatchUploadPromoHandler& operator=(const BatchUploadPromoHandler&) = delete;
  ~BatchUploadPromoHandler() override;

  void OnBatchUploadDialogClosed();

  // syncer::SyncServiceObserver implementation:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // batch_upload_promo::mojom::PageHandler:
  void GetBatchUploadPromoLocalDataCount(
      GetBatchUploadPromoLocalDataCountCallback callback) override;
  void OnBatchUploadPromoClicked() override;

 private:
  void OnLocalDataCountChanged(int32_t local_data_count);

  mojo::Receiver<batch_upload_promo::mojom::PageHandler> receiver_;
  mojo::Remote<batch_upload_promo::mojom::Page> page_;
  const raw_ref<Profile> profile_;
  raw_ref<BatchUploadService> batch_upload_service_;
  const raw_ptr<content::WebContents> web_contents_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::WeakPtrFactory<BatchUploadPromoHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BATCH_UPLOAD_PROMO_BATCH_UPLOAD_PROMO_HANDLER_H_
