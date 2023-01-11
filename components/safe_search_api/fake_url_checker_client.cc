// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/fake_url_checker_client.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace safe_search_api {

FakeURLCheckerClient::FakeURLCheckerClient() = default;

FakeURLCheckerClient::~FakeURLCheckerClient() = default;

void FakeURLCheckerClient::CheckURL(const GURL& url,
                                    ClientCheckCallback callback) {
  url_ = url;
  DCHECK(!callback_);
  callback_ = std::move(callback);
}

void FakeURLCheckerClient::RunCallback(ClientClassification classification) {
  std::move(callback_).Run(url_, classification);
}

void FakeURLCheckerClient::RunCallbackAsync(
    ClientClassification classification) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), url_, classification));
}

}  // namespace safe_search_api
