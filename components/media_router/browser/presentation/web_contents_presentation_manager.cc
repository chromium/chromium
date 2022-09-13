// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"

#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"

namespace media_router {

namespace {
WebContentsPresentationManager* g_test_instance = nullptr;
}  // namespace

// static
base::WeakPtr<WebContentsPresentationManager>
WebContentsPresentationManager::Get(content::WebContents* web_contents) {
  if (g_test_instance)
    return g_test_instance->GetWeakPtr();

  return PresentationServiceDelegateImpl::GetOrCreateForWebContents(
             web_contents)
      ->GetWeakPtr();
}

// static
void WebContentsPresentationManager::SetTestInstance(
    WebContentsPresentationManager* test_instance) {
  g_test_instance = test_instance;
}

WebContentsPresentationManager::~WebContentsPresentationManager() = default;

}  // namespace media_router
