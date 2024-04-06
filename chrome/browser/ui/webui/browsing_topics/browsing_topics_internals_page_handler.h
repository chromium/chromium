// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSING_TOPICS_BROWSING_TOPICS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSING_TOPICS_BROWSING_TOPICS_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browsing_topics/annotator.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

// Implements the mojo endpoint for the WebUI which proxies calls to the
// BrowsingTopicsService to get information about relevant topics state. Owned
// by BrowsingTopicsInternalsUI.
class BrowsingTopicsInternalsPageHandler
    : public browsing_topics::mojom::PageHandler {
 public:
  BrowsingTopicsInternalsPageHandler(
      Profile* profile,
      mojo::PendingReceiver<browsing_topics::mojom::PageHandler> receiver);
  BrowsingTopicsInternalsPageHandler(
      const BrowsingTopicsInternalsPageHandler&) = delete;
  BrowsingTopicsInternalsPageHandler& operator=(
      const BrowsingTopicsInternalsPageHandler&) = delete;
  ~BrowsingTopicsInternalsPageHandler() override;

  // browsing_topics::mojom::PageHandler overrides:
  void GetBrowsingTopicsConfiguration(
      browsing_topics::mojom::PageHandler::
          GetBrowsingTopicsConfigurationCallback callback) override;
  void GetBrowsingTopicsState(
      bool calculate_now,
      browsing_topics::mojom::PageHandler::GetBrowsingTopicsStateCallback
          callback) override;
  void GetModelInfo(browsing_topics::mojom::PageHandler::GetModelInfoCallback
                        callback) override;
  void ClassifyHosts(const std::vector<std::string>& hosts,
                     browsing_topics::mojom::PageHandler::ClassifyHostsCallback
                         callback) override;

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  void OnGetModelInfoCompleted(
      browsing_topics::mojom::PageHandler::GetModelInfoCallback callback);
  void OnGetTopicsForHostsCompleted(
      browsing_topics::mojom::PageHandler::ClassifyHostsCallback callback,
      const std::vector<browsing_topics::Annotation>& annotations);

  const raw_ptr<Profile> profile_;

  mojo::Receiver<browsing_topics::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<BrowsingTopicsInternalsPageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSING_TOPICS_BROWSING_TOPICS_INTERNALS_PAGE_HANDLER_H_
