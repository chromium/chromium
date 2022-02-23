// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace autofill {

class ContentAutofillDriver;

// Manages lifetime of ContentAutofillDriver. One Factory per WebContents
// creates one Driver per RenderFrame.
class ContentAutofillDriverFactory : public content::WebContentsObserver,
                                     public base::SupportsUserData::Data {
 public:
  static const char kContentAutofillDriverFactoryWebContentsUserDataKey[];

  // Creates a factory for a WebContents object.
  //
  // The |autofill_manager_factory_callback| is eventually called by
  // ContentAutofillDriver's constructor. Chrome passes a null callback and
  // ContentAutofillDriver calls SetBrowserAutofillManager() in this case.
  //
  // TODO(crbug.com/1200511): Remove default parameter and pass proper callback
  // and remove ContentAutofillDriver::SetBrowserAutofillManager().
  static void CreateForWebContentsAndDelegate(
      content::WebContents* contents,
      AutofillClient* client,
      const std::string& app_locale,
      BrowserAutofillManager::AutofillDownloadManagerState
          enable_download_manager,
      AutofillManager::AutofillManagerFactoryCallback
          autofill_manager_factory_callback = {});

  static ContentAutofillDriverFactory* FromWebContents(
      content::WebContents* contents);

  static void BindAutofillDriver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver,
      content::RenderFrameHost* render_frame_host);

  ~ContentAutofillDriverFactory() override;

  // Gets the |ContentAutofillDriver| associated with |render_frame_host|.
  // If |render_frame_host| is currently being deleted, this may be nullptr.
  // |render_frame_host| must be owned by |web_contents()|.
  ContentAutofillDriver* DriverForFrame(
      content::RenderFrameHost* render_frame_host);

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  AutofillClient* client() { return client_; }

 private:
  friend class ContentAutofillDriverFactoryTestApi;

  ContentAutofillDriverFactory(
      content::WebContents* web_contents,
      AutofillClient* client,
      const std::string& app_locale,
      BrowserAutofillManager::AutofillDownloadManagerState
          enable_download_manager,
      AutofillManager::AutofillManagerFactoryCallback
          autofill_manager_factory_callback);

  const raw_ptr<AutofillClient> client_;
  std::string app_locale_;
  BrowserAutofillManager::AutofillDownloadManagerState enable_download_manager_;
  AutofillManager::AutofillManagerFactoryCallback
      autofill_manager_factory_callback_;

  // Routes events between different drivers.
  // Must be destroyed after |driver_map_|'s elements.
  ContentAutofillRouter router_;

  // The list of drivers, one for each frame in the WebContents.
  // Should be empty at destruction time because its elements are erased in
  // RenderFrameDeleted(). In case it is not empty, is must be destroyed before
  // |router_| because ~ContentAutofillDriver() may access |router_|.
  std::unordered_map<content::RenderFrameHost*,
                     std::unique_ptr<ContentAutofillDriver>>
      driver_map_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_H_
