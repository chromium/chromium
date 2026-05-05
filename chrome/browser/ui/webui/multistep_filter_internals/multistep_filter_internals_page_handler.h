// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MULTISTEP_FILTER_INTERNALS_MULTISTEP_FILTER_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MULTISTEP_FILTER_INTERNALS_MULTISTEP_FILTER_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace multistep_filter_internals {

// Handler for the chrome://multistep-filter-internals WebUI page.
//
// This class is responsible for receiving requests from the WebUI page, and
// pushing log entries to the WebUI page by observing the log router.
class MultistepFilterInternalsPageHandler
    : public mojom::PageHandler,
      public multistep_filter::MultistepFilterLogRouter::Observer {
 public:
  MultistepFilterInternalsPageHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver,
      mojo::PendingRemote<mojom::Page> page,
      multistep_filter::MultistepFilterLogRouter* log_router);

  MultistepFilterInternalsPageHandler(
      const MultistepFilterInternalsPageHandler&) = delete;
  MultistepFilterInternalsPageHandler& operator=(
      const MultistepFilterInternalsPageHandler&) = delete;

  ~MultistepFilterInternalsPageHandler() override;

  // mojom::PageHandler:
  void GetBufferedLogs(GetBufferedLogsCallback callback) override;

  // multistep_filter::MultistepFilterLogRouter::Observer:
  void OnLogEntryAdded(const multistep_filter::LogEntry& entry) override;
  void OnLogRouterShutdown() override;

 private:
  raw_ptr<multistep_filter::MultistepFilterLogRouter> log_router_;
  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;

  base::ScopedObservation<multistep_filter::MultistepFilterLogRouter,
                          multistep_filter::MultistepFilterLogRouter::Observer>
      log_router_observation_{this};
};

}  // namespace multistep_filter_internals

#endif  // CHROME_BROWSER_UI_WEBUI_MULTISTEP_FILTER_INTERNALS_MULTISTEP_FILTER_INTERNALS_PAGE_HANDLER_H_
