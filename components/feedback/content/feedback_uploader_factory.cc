// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/content/feedback_uploader_factory.h"

#include "base/memory/singleton.h"
#include "components/feedback/feedback_uploader.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace feedback {

namespace {

// Helper function to create an URLLoaderFactory for the FeedbackUploader from
// the BrowserContext storage partition. As creating the storage partition can
// be expensive, this is delayed so that it does not happen during startup.
scoped_refptr<network::SharedURLLoaderFactory>
CreateURLLoaderFactoryForBrowserContext(content::BrowserContext* context) {
  return context->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace

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

std::unique_ptr<KeyedService>
FeedbackUploaderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // The returned FeedbackUploader lifetime is bound to that of BrowserContext
  // by the KeyedServiceFactory infrastructure. The FeedbackUploader will be
  // destroyed before the BrowserContext, thus base::Unretained() usage is safe.
  return std::make_unique<FeedbackUploader>(
      context->IsOffTheRecord(), context->GetPath(),
      base::BindOnce(&CreateURLLoaderFactoryForBrowserContext,
                     base::Unretained(context)));
}

content::BrowserContext* FeedbackUploaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace feedback
