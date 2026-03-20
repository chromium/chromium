// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_page_handler_impl.h"

#include "base/compiler_specific.h"

WebNNInternalsPageHandlerImpl::WebNNInternalsPageHandlerImpl(
    mojo::PendingReceiver<webnn_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<webnn_internals::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  observation_.Observe(content::WebNNIntrospectionManager::GetInstance());
}

WebNNInternalsPageHandlerImpl::~WebNNInternalsPageHandlerImpl() = default;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
void WebNNInternalsPageHandlerImpl::SetGraphRecordEnabled(bool enabled) {
  content::WebNNIntrospectionManager::GetInstance()->SetMLGraphRecordEnabled(
      enabled);
}

void WebNNInternalsPageHandlerImpl::IsGraphRecording(
    IsGraphRecordingCallback callback) {
  std::move(callback).Run(content::WebNNIntrospectionManager::GetInstance()
                              ->IsMLGraphRecordEnabled());
}

void WebNNInternalsPageHandlerImpl::OnGraphRecorded(
    const mojo_base::BigBuffer& json_data) {
  page_->ExportGraphRecorded(json_data.Clone());
}

void WebNNInternalsPageHandlerImpl::OnGraphRecordEnabledChanged(
    bool is_enabled) {
  page_->OnGraphRecordEnabledChanged(is_enabled);
}
#endif

void WebNNInternalsPageHandlerImpl::OnUpdateExistingContextDetails(
    const std::string& contexts_info_json) {}
