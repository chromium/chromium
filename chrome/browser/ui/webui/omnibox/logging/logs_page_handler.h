// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_LOGGING_LOGS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_LOGGING_LOGS_PAGE_HANDLER_H_

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/omnibox/logging/logs.mojom.h"
#include "components/omnibox/common/logger.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace omnibox::logging {

class LogsPageHandler : public omnibox::logging::mojom::PageHandler,
                        public omnibox::Logger::Observer {
 public:
  LogsPageHandler(
      mojo::PendingReceiver<omnibox::logging::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox::logging::mojom::Page> page);
  ~LogsPageHandler() override;

  LogsPageHandler(const LogsPageHandler&) = delete;
  LogsPageHandler& operator=(const LogsPageHandler&) = delete;

  // Logger::Observer impl:
  void OnLogMessageAdded(base::Time event_time,
                         const std::string& tag,
                         const std::string& source_file,
                         uint32_t source_line,
                         const std::string& message) override;

 private:
  mojo::Receiver<omnibox::logging::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox::logging::mojom::Page> page_;

  base::ScopedObservation<omnibox::Logger, omnibox::Logger::Observer>
      logger_observation_{this};
};

}  // namespace omnibox::logging

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_LOGGING_LOGS_PAGE_HANDLER_H_
