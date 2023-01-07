// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_COOKIE_WAITER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_COOKIE_WAITER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace ash {

class CookieWaiter : public network::mojom::CookieChangeListener {
 public:
  CookieWaiter(network::mojom::CookieManager* cookie_manager,
               const std::string& cookie_name,
               base::RepeatingClosure on_cookie_change,
               base::OnceClosure on_timeout);
  ~CookieWaiter() override;

  CookieWaiter(const CookieWaiter&) = delete;
  CookieWaiter& operator=(const CookieWaiter&) = delete;

  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo&) override;

 private:
  base::RepeatingClosure on_cookie_change_;

  mojo::Receiver<network::mojom::CookieChangeListener> cookie_listener_{this};
  base::OneShotTimer waiting_timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_COOKIE_WAITER_H_
