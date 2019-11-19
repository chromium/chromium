// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/form_submission_tracker_util.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"

namespace password_manager {

namespace {

const char kContentPasswordManagerDriverFactoryWebContentsUserDataKey[] =
    "web_contents_password_manager_driver_factory";

}  // namespace

void ContentPasswordManagerDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    PasswordManagerClient* password_client,
    autofill::AutofillClient* autofill_client) {
  if (FromWebContents(web_contents))
    return;

  // NOTE: Can't use |std::make_unique| due to private constructor.
  web_contents->SetUserData(
      kContentPasswordManagerDriverFactoryWebContentsUserDataKey,
      base::WrapUnique(new ContentPasswordManagerDriverFactory(
          web_contents, password_client, autofill_client)));
}

ContentPasswordManagerDriverFactory::ContentPasswordManagerDriverFactory(
    content::WebContents* web_contents,
    PasswordManagerClient* password_client,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      password_client_(password_client),
      autofill_client_(autofill_client) {}

ContentPasswordManagerDriverFactory::~ContentPasswordManagerDriverFactory() {}

// static
ContentPasswordManagerDriverFactory*
ContentPasswordManagerDriverFactory::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentPasswordManagerDriverFactory*>(
      contents->GetUserData(
          kContentPasswordManagerDriverFactoryWebContentsUserDataKey));
}

// static
void ContentPasswordManagerDriverFactory::BindPasswordManagerDriver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // We try to bind to the driver of this render frame host,
  // but if driver is not ready for this render frame host for now,
  // the request will be just dropped, this would cause closing the message pipe
  // which would raise connection error to peer side.
  // Peer side could reconnect later when needed.
  if (!web_contents)
    return;

  ContentPasswordManagerDriverFactory* factory =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);
  if (!factory)
    return;

  factory->GetDriverForFrame(render_frame_host)
      ->BindPendingReceiver(std::move(pending_receiver));
}

ContentPasswordManagerDriver*
ContentPasswordManagerDriverFactory::GetDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_EQ(web_contents(),
            content::WebContents::FromRenderFrameHost(render_frame_host));
  DCHECK(render_frame_host->IsRenderFrameCreated());

  auto& driver = frame_driver_map_[render_frame_host];
  if (!driver) {
    driver = std::make_unique<ContentPasswordManagerDriver>(
        render_frame_host, password_client_, autofill_client_);
  }
  return driver.get();
}

void ContentPasswordManagerDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

void ContentPasswordManagerDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation->IsInMainFrame() || navigation->IsSameDocument() ||
      !navigation->HasCommitted()) {
    return;
  }

  // Clear page specific data after main frame navigation.
  NotifyDidNavigateMainFrame(navigation->IsRendererInitiated(),
                             navigation->GetPageTransition(),
                             navigation->WasInitiatedByLinkClick(),
                             password_client_->GetPasswordManager());
  GetDriverForFrame(navigation->GetRenderFrameHost())
      ->GetPasswordAutofillManager()
      ->DidNavigateMainFrame();
}

void ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability() {
  for (const auto& key_val_iterator : frame_driver_map_) {
    key_val_iterator.second->SendLoggingAvailability();
  }
}

}  // namespace password_manager
