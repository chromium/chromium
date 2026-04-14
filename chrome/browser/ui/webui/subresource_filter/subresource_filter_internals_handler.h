// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_INTERNALS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace subresource_filter {

// Handles requests from, and dispatches observer notifications to, the
// chrome://subresource-filter-internals WebUI. This page is used by developers
// to manage global debugging and diagnostic settings for the subresource filter
// (e.g., ad highlighting).
class SubresourceFilterInternalsHandler
    : public mojom::SubresourceFilterInternalsHandler {
 public:
  explicit SubresourceFilterInternalsHandler(Profile* profile);

  SubresourceFilterInternalsHandler(const SubresourceFilterInternalsHandler&) =
      delete;
  SubresourceFilterInternalsHandler& operator=(
      const SubresourceFilterInternalsHandler&) = delete;

  ~SubresourceFilterInternalsHandler() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SubresourceFilterInternalsHandler> receiver);

  // mojom::SubresourceFilterInternalsHandler:
  void GetInternalsPageSettings(
      GetInternalsPageSettingsCallback callback) override;
  void SetInternalsPageSettings(
      mojom::SubresourceFilterInternalsPageSettingsPtr settings) override;
  void ObserveInternalsPageSettings(
      mojo::PendingRemote<mojom::SubresourceFilterInternalsObserver> observer)
      override;

 private:
  void OnPrefChanged();

  mojo::Receiver<mojom::SubresourceFilterInternalsHandler> receiver_{this};
  mojo::Remote<mojom::SubresourceFilterInternalsObserver> observer_;

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_UI_WEBUI_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_INTERNALS_HANDLER_H_
