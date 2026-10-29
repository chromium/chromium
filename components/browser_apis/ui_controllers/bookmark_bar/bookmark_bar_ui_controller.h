// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_H_

#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace bookmark_bar_api {

class BookmarkBarUIControllerObserver;

// Browser actions and state for a bookmark bar. The bookmark bar UI owns the
// controller. In-process clients call the direct return stub; out-of-process
// clients connect through Accept(). Methods that take node IDs must only be
// called once bookmarks have loaded.
class BookmarkBarUIController
    : public mojom::BookmarkBarUIControllerDirectReturnStub {
 public:
  ~BookmarkBarUIController() override = default;

  // Additional API for synchronous clients:
  // Returns the current display options without subscribing to a Mojo stream.
  // Use AddObserver() to get updates.
  virtual mojom::DisplayOptionsPtr GetDisplayOptionsWithoutObservation() = 0;
  virtual void AddObserver(BookmarkBarUIControllerObserver* observer) = 0;
  virtual void RemoveObserver(BookmarkBarUIControllerObserver* observer) = 0;

  // Mojo handling:
  virtual void Accept(
      mojo::PendingReceiver<mojom::BookmarkBarUIController> receiver) = 0;
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_H_
