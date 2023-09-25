// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_url_handler_manager.h"

#include "chrome/browser/profiles/profile.h"

namespace web_app {

FakeUrlHandlerManager::FakeUrlHandlerManager(Profile* profile)
    : UrlHandlerManager(profile) {}

FakeUrlHandlerManager::~FakeUrlHandlerManager() = default;

void FakeUrlHandlerManager::RegisterUrlHandlers(const webapps::AppId& app_id,
                                                ResultCallback callback) {
  std::move(callback).Run(Result::kOk);
}

bool FakeUrlHandlerManager::UnregisterUrlHandlers(
    const webapps::AppId& app_id) {
  return true;
}

void FakeUrlHandlerManager::UpdateUrlHandlers(
    const webapps::AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  std::move(callback).Run(true);
}

}  // namespace web_app
