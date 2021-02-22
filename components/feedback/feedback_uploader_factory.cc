// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader_factory.h"

#include "base/memory/singleton.h"
#include "components/feedback/feedback_uploader.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace feedback {

// static
FeedbackUploaderFactory* FeedbackUploaderFactory::GetInstance() {
  return base::Singleton<FeedbackUploaderFactory>::get();
}

// static
FeedbackUploader* FeedbackUploaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FeedbackUploader*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

FeedbackUploaderFactory::FeedbackUploaderFactory(const char* service_name)
    : BrowserContextKeyedServiceFactory(
          service_name,
          BrowserContextDependencyManager::GetInstance()) {}

FeedbackUploaderFactory::FeedbackUploaderFactory()
    : BrowserContextKeyedServiceFactory(
          "feedback::FeedbackUploader",
          BrowserContextDependencyManager::GetInstance()) {}

FeedbackUploaderFactory::~FeedbackUploaderFactory() {}

KeyedService* FeedbackUploaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FeedbackUploader(context);
}

content::BrowserContext* FeedbackUploaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace feedback
