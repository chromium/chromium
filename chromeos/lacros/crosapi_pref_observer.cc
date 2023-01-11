// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/crosapi_pref_observer.h"

#include "base/functional/callback.h"
#include "chromeos/lacros/lacros_service.h"

CrosapiPrefObserver::CrosapiPrefObserver(crosapi::mojom::PrefPath path,
                                         PrefChangedCallback callback)
    : callback_(std::move(callback)) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->AddObserver(
      path, receiver_.BindNewPipeAndPassRemoteWithVersion());
}

CrosapiPrefObserver::~CrosapiPrefObserver() = default;

void CrosapiPrefObserver::OnPrefChanged(base::Value value) {
  callback_.Run(std::move(value));
}
