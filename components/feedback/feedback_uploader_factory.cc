// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader_factory.h"

#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
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

// static
scoped_refptr<base::SingleThreadTaskRunner>
FeedbackUploaderFactory::CreateUploaderTaskRunner() {
  // Uses a BLOCK_SHUTDOWN file task runner because we really don't want to
  // lose reports or corrupt their files.
  return base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

FeedbackUploaderFactory::FeedbackUploaderFactory(const char* service_name)
    : BrowserContextKeyedServiceFactory(
          service_name,
          BrowserContextDependencyManager::GetInstance()),
      task_runner_(CreateUploaderTaskRunner()) {}

FeedbackUploaderFactory::FeedbackUploaderFactory()
    : BrowserContextKeyedServiceFactory(
          "feedback::FeedbackUploader",
          BrowserContextDependencyManager::GetInstance()),
      task_runner_(CreateUploaderTaskRunner()) {}

FeedbackUploaderFactory::~FeedbackUploaderFactory() {}

KeyedService* FeedbackUploaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FeedbackUploader(context, task_runner_);
}

content::BrowserContext* FeedbackUploaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace feedback
