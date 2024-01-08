// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_CONTACT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_CONTACT_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace content {
class BrowserContext;
}  // namespace content

// WebUIMessageHandler for Contact Messages to pass messages to the
// chrome://nearby-internals Contact tab.
class NearbyInternalsContactHandler
    : public content::WebUIMessageHandler,
      public NearbyShareContactManager::Observer {
 public:
  explicit NearbyInternalsContactHandler(content::BrowserContext* context);
  NearbyInternalsContactHandler(const NearbyInternalsContactHandler&) = delete;
  NearbyInternalsContactHandler& operator=(
      const NearbyInternalsContactHandler&) = delete;
  ~NearbyInternalsContactHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Message handler callback that initializes JavaScript.
  void InitializeContents(const base::Value::List& args);

  // NearbyShareContactManager::Observer:
  void OnContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out) override;
  void OnContactsUploaded(bool did_contacts_change_since_last_upload) override;

  // Message handler callback that requests a contacts download from the contact
  // manager.
  void HandleDownloadContacts(const base::Value::List& args);

  raw_ptr<content::BrowserContext> context_;
  base::ScopedObservation<NearbyShareContactManager,
                          NearbyShareContactManager::Observer>
      observation_{this};
  base::WeakPtrFactory<NearbyInternalsContactHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_CONTACT_HANDLER_H_
