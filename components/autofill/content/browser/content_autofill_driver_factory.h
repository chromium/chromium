// Copyright 2014 The Chromium Authors
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

// Creates an BrowserAutofillManager and attaches it to the `driver`.
//
// This hook is to be passed to CreateForWebContentsAndDelegate().
// It is the glue between ContentAutofillDriver[Factory] and
// BrowserAutofillManager.
//
// Other embedders (which don't want to use BrowserAutofillManager) shall use
// other implementations.
void BrowserDriverInitHook(AutofillClient* client,
                           const std::string& app_locale,
                           ContentAutofillDriver* driver);

// Manages lifetime of ContentAutofillDriver. One Factory per WebContents
// creates one Driver per RenderFrame.
class ContentAutofillDriverFactory : public content::WebContentsObserver,
                                     public base::SupportsUserData::Data {
 public:
  using DriverInitCallback =
      base::RepeatingCallback<void(ContentAutofillDriver*)>;

  static const char kContentAutofillDriverFactoryWebContentsUserDataKey[];

  // Creates a factory for a WebContents object.
  //
  // The `driver_init_hook` is called whenever a driver is constructed, so it
  // may configure the driver. In particular, it must create and set the
  // driver's AutofillManager.
  static void CreateForWebContentsAndDelegate(
      content::WebContents* contents,
      AutofillClient* client,
      DriverInitCallback driver_init_hook);

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

  AutofillClient* client() { return client_; }

 private:
  friend class ContentAutofillDriverFactoryTestApi;

  ContentAutofillDriverFactory(content::WebContents* web_contents,
                               AutofillClient* client,
                               DriverInitCallback driver_init_hook);

  std::unique_ptr<ContentAutofillDriver> CreateDriver(
      content::RenderFrameHost* rfh);

  const raw_ptr<AutofillClient, DanglingUntriaged> client_;
  DriverInitCallback driver_init_hook_;

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
