// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace web_app {

TestWebAppUrlLoader::TestWebAppUrlLoader() = default;

TestWebAppUrlLoader::~TestWebAppUrlLoader() = default;

void TestWebAppUrlLoader::SaveLoadUrlRequests() {
  should_save_requests_ = true;
}

void TestWebAppUrlLoader::ProcessLoadUrlRequests() {
  while (!pending_requests_.empty()) {
    GURL url;
    ResultCallback callback;

    std::tie(url, callback) = std::move(pending_requests_.front());
    pending_requests_.pop();

    DCHECK(base::Contains(next_result_map_, url));
    auto result = next_result_map_[url];
    next_result_map_.erase(url);

    std::move(callback).Run(result);
  }
}

void TestWebAppUrlLoader::SetNextLoadUrlResult(const GURL& url, Result result) {
  DCHECK(!base::Contains(next_result_map_, url)) << url;
  next_result_map_[url] = result;
}

void TestWebAppUrlLoader::LoadUrl(const GURL& url,
                                  content::WebContents* web_contents,
                                  UrlComparison url_comparison,
                                  ResultCallback callback) {
  if (should_save_requests_) {
    pending_requests_.emplace(url, std::move(callback));
    return;
  }

  DCHECK(base::Contains(next_result_map_, url)) << url;
  auto result = next_result_map_[url];
  next_result_map_.erase(url);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace web_app
