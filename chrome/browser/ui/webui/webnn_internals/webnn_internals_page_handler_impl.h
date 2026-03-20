// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals.mojom.h"
#include "content/public/browser/webnn_introspection_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"

class WebNNInternalsPageHandlerImpl
    : public webnn_internals::mojom::PageHandler,
      public content::WebNNIntrospectionManager::Observer {
 public:
  WebNNInternalsPageHandlerImpl(
      mojo::PendingReceiver<webnn_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<webnn_internals::mojom::Page> page);

  WebNNInternalsPageHandlerImpl(const WebNNInternalsPageHandlerImpl&) = delete;
  WebNNInternalsPageHandlerImpl& operator=(
      const WebNNInternalsPageHandlerImpl&) = delete;

  ~WebNNInternalsPageHandlerImpl() override;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // webnn_internals::mojom::WebNNInternalsHandler
  void SetGraphRecordEnabled(bool enabled) override;

  // webnn_internals::mojom::WebNNInternalsHandler
  void IsGraphRecording(IsGraphRecordingCallback callback) override;

  // webnn::WebNNIntrospectionManager::Observer:
  void OnGraphRecorded(const mojo_base::BigBuffer& json_data) override;
  void OnGraphRecordEnabledChanged(bool is_enabled) override;
#endif
  // webnn::WebNNIntrospectionManager::Observer:
  void OnUpdateExistingContextDetails(
      const std::string& contexts_info_json) override;

 private:
  mojo::Receiver<webnn_internals::mojom::PageHandler> receiver_;
  mojo::Remote<webnn_internals::mojom::Page> page_;

  base::ScopedObservation<content::WebNNIntrospectionManager,
                          content::WebNNIntrospectionManager::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_PAGE_HANDLER_IMPL_H_
