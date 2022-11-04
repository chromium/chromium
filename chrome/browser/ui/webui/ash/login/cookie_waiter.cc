// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/cookie_waiter.h"

#include "google_apis/gaia/gaia_urls.h"

namespace ash {

namespace {

constexpr base::TimeDelta kCookieDelay = base::Seconds(20);
}

CookieWaiter::CookieWaiter(network::mojom::CookieManager* cookie_manager,
                           const std::string& cookie_name,
                           base::RepeatingClosure on_cookie_change,
                           base::OnceClosure on_timeout)
    : on_cookie_change_(std::move(on_cookie_change)) {
  cookie_manager->AddCookieChangeListener(
      GaiaUrls::GetInstance()->gaia_url(), cookie_name,
      cookie_listener_.BindNewPipeAndPassRemote());
  waiting_timer_.Start(FROM_HERE, kCookieDelay, std::move(on_timeout));
}

CookieWaiter::~CookieWaiter() = default;

void CookieWaiter::OnCookieChange(const net::CookieChangeInfo&) {
  on_cookie_change_.Run();
}

}  // namespace ash
