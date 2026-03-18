// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_handler.h"

#include "base/compiler_specific.h"

WebNNInternalsHandler::WebNNInternalsHandler(
    mojo::PendingReceiver<webnn_internals::mojom::WebNNInternalsHandler>
        receiver,
    mojo::PendingRemote<webnn_internals::mojom::WebNNInternalsPage> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  observation_.Observe(content::WebNNIntrospectionManager::GetInstance());
}

WebNNInternalsHandler::~WebNNInternalsHandler() = default;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
void WebNNInternalsHandler::SetGraphRecordEnabled(bool enabled) {
  content::WebNNIntrospectionManager::GetInstance()->SetMLGraphRecordEnabled(
      enabled);
}

void WebNNInternalsHandler::IsGraphRecording(
    IsGraphRecordingCallback callback) {
  std::move(callback).Run(content::WebNNIntrospectionManager::GetInstance()
                              ->IsMLGraphRecordEnabled());
}

void WebNNInternalsHandler::OnGraphRecorded(
    const mojo_base::BigBuffer& json_data) {
  page_->ExportGraphRecorded(json_data.Clone());
}

void WebNNInternalsHandler::OnGraphRecordEnabledChanged(bool is_enabled) {
  page_->OnGraphRecordEnabledChanged(is_enabled);
}
#endif

void WebNNInternalsHandler::OnUpdateExistingContextDetails(
    const std::string& contexts_info_json) {}
