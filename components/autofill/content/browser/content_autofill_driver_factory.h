// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace autofill {

class ContentAutofillClient;
class ContentAutofillDriver;
class ScopedAutofillManagersObservation;

// Manages lifetime of ContentAutofillDriver. Owned by ContentAutofillClient,
// therefore one Factory per WebContents. Creates one Driver per
// RenderFrameHost.
class ContentAutofillDriverFactory : public content::WebContentsObserver {
 public:
  // Observer of ContentAutofillDriverFactory events.
  //
  // Using this observer is preferable over registering a WebContentsObserver
  // and calling ContentAutofillDriverFactory::DriverForFrame() in the
  // WebContentsObserver events.
  class Observer : public base::CheckedObserver {
   public:
    // Called during destruction of the ContentAutofillDriverFactory. It can,
    // e.g., be used to reset `ScopedObservation`s observing `this`.
    virtual void OnContentAutofillDriverFactoryDestroyed(
        ContentAutofillDriverFactory& factory) {}

    // Called right after the driver has been created.
    // At the time of this event, the `driver` object is already fully alive and
    // `factory.DriverForFrame(driver.render_frame_host()) == &driver` holds.
    virtual void OnContentAutofillDriverCreated(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver) {}

    // Called right before the driver's RenderFrameHost is deleted.
    // At the time of this event, the `driver` object is still fully alive and
    // `factory.DriverForFrame(driver.render_frame_host()) == &driver` holds.
    virtual void OnContentAutofillDriverWillBeDeleted(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver) {}
  };

  static ContentAutofillDriverFactory* FromWebContents(
      content::WebContents* contents);

  static void BindAutofillDriver(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver);

  ContentAutofillDriverFactory(content::WebContents* web_contents,
                               ContentAutofillClient* client);
  ContentAutofillDriverFactory(ContentAutofillDriverFactory&) = delete;
  ContentAutofillDriverFactory& operator=(ContentAutofillDriverFactory&) =
      delete;
  ~ContentAutofillDriverFactory() override;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  ContentAutofillClient& client() { return *client_; }

  AutofillDriverRouter& router() { return router_; }

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  size_t num_drivers() const { return driver_map_.size(); }

  // Returns raw pointers to all drivers that the factory currently owns.
  std::vector<ContentAutofillDriver*> GetExistingDrivers(
      base::PassKey<ScopedAutofillManagersObservation>);

  ContentAutofillDriver* DriverForFrame(
      content::RenderFrameHost* render_frame_host,
      base::PassKey<ContentAutofillDriver>) {
    return DriverForFrame(render_frame_host);
  }

 private:
  friend class ContentAutofillDriverFactoryTestApi;

  // Gets the `ContentAutofillDriver` associated with `render_frame_host`.
  // If `render_frame_host` is currently being deleted, this may be nullptr.
  // `render_frame_host` must be owned by `web_contents()`.
  ContentAutofillDriver* DriverForFrame(
      content::RenderFrameHost* render_frame_host);

  // The owning AutofillClient.
  const raw_ref<ContentAutofillClient> client_;

  // Routes events between different ContentAutofillDrivers.
  // Must be destroyed after |driver_map_|'s elements.
  AutofillDriverRouter router_;

  // Owns the drivers, one for each frame in the WebContents.
  // Should be empty at destruction time because its elements are erased in
  // RenderFrameDeleted(). In case it is not empty, is must be destroyed before
  // |router_| because ~ContentAutofillDriver() may access |router_|.
  base::flat_map<content::RenderFrameHost*,
                 std::unique_ptr<ContentAutofillDriver>>
      driver_map_;

  base::ObserverList<Observer> observers_;

  // The maximum number of coexisting drivers over the lifetime of this factory.
  // TODO: crbug.com/342132628 - Remove the counter and the metric.
  size_t max_drivers_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_H_
