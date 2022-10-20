// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace content {
class WebContents;
}

namespace password_manager {

class ContentPasswordManagerDriver;

// Creates and owns ContentPasswordManagerDrivers. There is one
// factory per WebContents, and one driver per RenderFrameHost.
class ContentPasswordManagerDriverFactory
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContentPasswordManagerDriverFactory> {
 public:
  ContentPasswordManagerDriverFactory(
      const ContentPasswordManagerDriverFactory&) = delete;
  ContentPasswordManagerDriverFactory& operator=(
      const ContentPasswordManagerDriverFactory&) = delete;

  ~ContentPasswordManagerDriverFactory() override;

  static void BindPasswordManagerDriver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
          pending_receiver,
      content::RenderFrameHost* render_frame_host);

  // Note that this may return null if the RenderFrameHost does not have a
  // live RenderFrame (e.g. it represents a crashed RenderFrameHost).
  ContentPasswordManagerDriver* GetDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  // Requests all drivers to inform their renderers whether
  // chrome://password-manager-internals is available.
  void RequestSendLoggingAvailability();

 private:
  friend class content::WebContentsUserData<
      ContentPasswordManagerDriverFactory>;
  friend class ContentPasswordManagerDriverFactoryTestApi;

  ContentPasswordManagerDriverFactory(
      content::WebContents* web_contents,
      PasswordManagerClient* client,
      autofill::AutofillClient* autofill_client);

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::map<content::RenderFrameHost*, ContentPasswordManagerDriver>
      frame_driver_map_;

  raw_ptr<PasswordManagerClient, DanglingUntriaged> password_client_;
  raw_ptr<autofill::AutofillClient, DanglingUntriaged> autofill_client_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_
