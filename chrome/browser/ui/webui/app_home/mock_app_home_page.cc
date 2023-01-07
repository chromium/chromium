// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/mock_app_home_page.h"

namespace webapps {

MockAppHomePage::MockAppHomePage() = default;

MockAppHomePage::~MockAppHomePage() = default;

mojo::PendingRemote<app_home::mojom::Page> MockAppHomePage::BindAndGetRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace webapps
