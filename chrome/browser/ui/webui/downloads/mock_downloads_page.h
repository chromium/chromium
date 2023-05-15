// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_

#include <vector>

#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPage : public downloads::mojom::Page {
 public:
  MockPage();
  ~MockPage() override;

  mojo::PendingRemote<downloads::mojom::Page> BindAndGetRemote();

  MOCK_METHOD(void, RemoveItem, (int));
  MOCK_METHOD(void, UpdateItem, (int, downloads::mojom::DataPtr));
  MOCK_METHOD(void, InsertItems, (int, std::vector<downloads::mojom::DataPtr>));
  MOCK_METHOD(void, ClearAll, ());

  mojo::Receiver<downloads::mojom::Page> receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_
