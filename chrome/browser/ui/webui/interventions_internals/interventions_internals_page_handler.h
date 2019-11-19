// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_logger_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_quality_tracker.h"

class InterventionsInternalsPageHandler
    : public previews::PreviewsLoggerObserver,
      public network::NetworkQualityTracker::EffectiveConnectionTypeObserver,
      public mojom::InterventionsInternalsPageHandler {
 public:
  InterventionsInternalsPageHandler(
      mojo::PendingReceiver<mojom::InterventionsInternalsPageHandler> receiver,
      previews::PreviewsUIService* previews_ui_service,
      network::NetworkQualityTracker* network_quality_tracker);
  ~InterventionsInternalsPageHandler() override;

  // mojom::InterventionsInternalsPageHandler:
  void GetPreviewsEnabled(GetPreviewsEnabledCallback callback) override;
  void GetPreviewsFlagsDetails(
      GetPreviewsFlagsDetailsCallback callback) override;
  void SetClientPage(
      mojo::PendingRemote<mojom::InterventionsInternalsPage> page) override;
  void SetIgnorePreviewsBlacklistDecision(bool ignore) override;

  // previews::PreviewsLoggerObserver:
  void OnNewMessageLogAdded(
      const previews::PreviewsLogger::MessageLog& message) override;
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override;
  void OnUserBlacklistedStatusChange(bool blacklisted) override;
  void OnBlacklistCleared(base::Time time) override;
  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override;
  void OnLastObserverRemove() override;

 private:
  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  mojo::Receiver<mojom::InterventionsInternalsPageHandler> receiver_;

  // The PreviewsLogger that this handler is listening to, and guaranteed to
  // outlive |this|.
  previews::PreviewsLogger* logger_;

  // A pointer to the PreviewsUIService associated with this handler, and
  // guaranteed to outlive |this|.
  previews::PreviewsUIService* previews_ui_service_;

  // Passed in during construction. If null, the main browser process tracker
  // will be used instead.
  network::NetworkQualityTracker* network_quality_tracker_;

  // The current estimated effective connection type.
  net::EffectiveConnectionType current_estimated_ect_;

  // Handle back to the page by which we can pass in new log messages.
  mojo::Remote<mojom::InterventionsInternalsPage> page_;

  DISALLOW_COPY_AND_ASSIGN(InterventionsInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_
