// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_page_handler_impl.h"

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom.h"

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
    const std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>&
        contexts_details) {
  page_->OnUpdateExistingContextDetails(mojo::Clone(contexts_details));
}

void WebNNInternalsPageHandlerImpl::OnUpdateAvailableExecutionProvidersDetails(
    const std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>&
        available_execution_providers) {
  page_->OnUpdateAvailableExecutionProvidersDetails(
      mojo::Clone(available_execution_providers));
}

void WebNNInternalsPageHandlerImpl::RequestExistingContextsDetails(
    RequestExistingContextsDetailsCallback callback) {
  content::WebNNIntrospectionManager::GetInstance()
      ->EstablishServiceConnectionAndGetExistingContextsDetails(
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              std::vector<
                  webnn::mojom::WebNNContextIntrospectionDetailsPtr>()));
}

void WebNNInternalsPageHandlerImpl::RequestAvailableExecutionProvidersDetails(
    RequestAvailableExecutionProvidersDetailsCallback callback) {
  content::WebNNIntrospectionManager::GetInstance()
      ->EstablishServiceConnectionAndGetAvailableExecutionProvidersDetails(
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>()));
}

#if BUILDFLAG(IS_WIN)
void WebNNInternalsPageHandlerImpl::ForceOrtEnvironmentCreationForIntrospection(
    ForceOrtEnvironmentCreationForIntrospectionCallback callback) {
  content::WebNNIntrospectionManager::GetInstance()
      ->ForceOrtEnvironmentCreationForIntrospection(
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>()));
}
#endif
