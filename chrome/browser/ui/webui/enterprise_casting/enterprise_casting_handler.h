// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ENTERPRISE_CASTING_ENTERPRISE_CASTING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ENTERPRISE_CASTING_ENTERPRISE_CASTING_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/webui/enterprise_casting/enterprise_casting.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class EnterpriseCastingHandler : public enterprise_casting::mojom::PageHandler {
 public:
  EnterpriseCastingHandler(
      mojo::PendingReceiver<enterprise_casting::mojom::PageHandler>
          page_handler,
      mojo::PendingRemote<enterprise_casting::mojom::Page> page);
  ~EnterpriseCastingHandler() override;

  // enterprise_casting::mojom::PageHandler overrides:
  void AddSink(const std::string& access_code,
               enterprise_casting::mojom::CastDiscoveryMethod discovery_method,
               AddSinkCallback callback) override;

  // enterprise_casting::mojom::PageHandler overrides:
  void CastToSink(CastToSinkCallback callback) override;

 private:
  mojo::Remote<enterprise_casting::mojom::Page> page_;
  mojo::Receiver<enterprise_casting::mojom::PageHandler> receiver_;

  // The dispatcher only needs to cast to the most recent sink that was
  // added. Store this value after the call to add is made.
  const std::string recent_sink_id;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ENTERPRISE_CASTING_ENTERPRISE_CASTING_HANDLER_H_
