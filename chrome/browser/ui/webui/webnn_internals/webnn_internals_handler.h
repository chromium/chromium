// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals.mojom.h"
#include "chrome/browser/webnn/webnn_introspection_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"

class WebNNInternalsHandler
    : public webnn_internals::mojom::WebNNInternalsHandler,
      public webnn::WebNNIntrospectionManager::Observer {
 public:
  WebNNInternalsHandler(
      mojo::PendingReceiver<webnn_internals::mojom::WebNNInternalsHandler>
          receiver,
      mojo::PendingRemote<webnn_internals::mojom::WebNNInternalsPage> page);

  WebNNInternalsHandler(const WebNNInternalsHandler&) = delete;
  WebNNInternalsHandler& operator=(const WebNNInternalsHandler&) = delete;

  ~WebNNInternalsHandler() override;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  void SetGraphRecordEnabled(bool enabled) override;

  void IsGraphRecording(IsGraphRecordingCallback callback) override;

  // webnn::WebNNIntrospectionManager::Observer:
  void OnGraphRecorded(const mojo_base::BigBuffer& json_data) override;
  void OnGraphRecordEnabledChanged(bool is_enabled) override;
#endif

 private:
  mojo::Receiver<webnn_internals::mojom::WebNNInternalsHandler> receiver_;
  mojo::Remote<webnn_internals::mojom::WebNNInternalsPage> page_;

  base::ScopedObservation<webnn::WebNNIntrospectionManager,
                          webnn::WebNNIntrospectionManager::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_HANDLER_H_
